/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "audio/audioManager.h"
#include "lib/logger.h"
#include "audioManagerPrivate.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "lib/math.h"

#include <libdragon.h>
#include <array>
#include <cmath>

namespace
{
  constexpr uint32_t CHANNEL_COUNT = 32;
  constexpr uint32_t EMITTER_COUNT = 32;

  constexpr float MIN_SOURCE_DIST = 1e-4f;
  constexpr float CULL_TIME = 0.1f;

  constinit uint16_t nextUUID{1};

  struct Slot
  {
    union
    {
      wav64_t* audioWAV{nullptr};
      xm64player_t* audioXM;
    };
    float volume{1.0f};
    float speed{1.0f};
    uint16_t uuid{0};
    uint8_t isXM{0};
    int8_t emitter{-1};

    [[nodiscard]] bool hasAudio() const { return audioWAV != nullptr || audioXM != nullptr; }
    void clear() { *this = Slot{}; }
  };

  struct Emitter
  {
    wav64_t *audio{};
    P64::Audio::Conf3D conf{};
    fm_vec3_t pos{};
    float volume{1.0f};
    float speed{1.0f};
    float time{0.0f}; // playback time in seconds, to resume loops
    float outOfRangeTime{0.0f};
    int8_t ch{-1};
    bool loop{};
    uint16_t uuid{0};

    [[nodiscard]] bool isActive() const { return audio != nullptr; }
  };

  struct Listener
  {
    fm_vec3_t pos{};
    fm_vec3_t right{1.0f, 0.0f, 0.0f};
  };

  std::array<Slot, CHANNEL_COUNT> slots{};
  std::array<Emitter, EMITTER_COUNT> emitters{};
  constinit Listener listener{};
  constinit P64::AudioManager::ListenerSource listenerSource{};
  constinit bool hasListenerSource{false};

  // @TODO: move out of here to avoid scene dep.
  void resolveListenerSource()
  {
    if(!hasListenerSource)return;
    hasListenerSource = false;

    auto &scene = P64::SceneManager::getCurrent();
    auto posObj = scene.getObjectById(listenerSource.posObjId);
    fm_vec3_t pos = posObj ? posObj->pos : listener.pos;

    if(listenerSource.rotObjId == 0) {
      auto cam = scene.getCamera();
      if(!cam) {
        listener.pos = pos;
        return;
      }
      P64::AudioManager::setListener(pos, cam->getViewDir(), cam->getUp());
    } else {
      auto rotObj = scene.getObjectById(listenerSource.rotObjId);
      if(!rotObj) {
        listener.pos = pos;
        return;
      }
      P64::AudioManager::setListener(pos, rotObj->rot * P64::Math::VEC3_FORWARD, rotObj->rot * P64::Math::VEC3_UP);
    }
  }

  void updateVolume(uint32_t i) {
    auto& slot = slots[i];
    if(slot.isXM) {
      xm64player_set_vol(slot.audioXM, slot.volume * P64::AudioManager::masterVol);
    } else {
      mixer_ch_set_vol(i,
        slot.volume * P64::AudioManager::masterVol,
        slot.volume * P64::AudioManager::masterVol
      );
    }
  }

  int32_t getFreeSlots(int count = 1) {
    for(uint32_t i=0; i<slots.size(); ++i) {
      bool free = true;
      for(int j=0; j<count; ++j) {
        if(i+j >= slots.size() || slots[i+j].hasAudio()) {
          free = false;
          break;
        }
      }
      if(free)return (int32_t)i;
    }
    return -1;
  }

  void playWav(wav64_t *audio, int ch, bool loop) {
    wav64_play(audio, ch);
    if(audio->wave.loop_len != 0) {
      mixer_ch_set_loop(ch, loop);
    }
  }

  void freeChannels(uint32_t ch, uint16_t uuid) {
    mixer_ch_stop(ch);
    for(; ch < CHANNEL_COUNT && slots[ch].uuid == uuid; ++ch) {
      slots[ch].clear();
    }
  }

  void releaseEmitterChannel(Emitter &e) {
    if(e.ch < 0)return;

    e.time = (float)mixer_ch_get_pos(e.ch) / e.audio->wave.frequency;
    freeChannels(e.ch, e.uuid);
    e.ch = -1;
  }

  void freeEmitter(Emitter &e) {
    if(e.ch >= 0) {
      freeChannels(e.ch, e.uuid);
    }
    e = Emitter{};
  }

  bool startEmitterChannel(Emitter &e, uint32_t idx) {
    auto slot = getFreeSlots(e.audio->wave.channels);
    if(slot < 0)return false;

    for(int s=slot; s < slot + e.audio->wave.channels; ++s) 
    {
      slots[s].audioWAV = e.audio;
      slots[s].uuid = e.uuid;
      slots[s].emitter = (int8_t)idx;
    }

    playWav(e.audio, slot, e.loop);
    if(e.time > 0.0f) 
    {
      float duration = (float)e.audio->wave.len / e.audio->wave.frequency;
      wav64_seek(e.audio, slot, fmodf(e.time, duration));
    }
    e.ch = (int8_t)slot;
    return true;
  }

  void updateEmitterChannel(Emitter &e)
  {
    const auto &cf = e.conf;
    fm_vec3_t to = e.pos - listener.pos;
    float dist2 = fm_vec3_len2(&to);

    float gain = 0.0f;
    float pan = 0.0f;

    if(dist2 < (cf.maxDist * cf.maxDist))
    {
      float dist = sqrtf(dist2);
      float range = cf.maxDist - cf.nearDist;
      float distT = range > 0.0f ? ((dist - cf.nearDist) / range) : 0.0f;

      distT = P64::Math::clamp(distT, 0.0f, 1.0f);

      if(cf.falloff) {
        gain = fmaxf(cf.falloff(distT, cf.rolloff), 0.0f);
      } else {
        // inverse-distance rescaled to actually reach zero
        float div = cf.nearDist + (cf.rolloff * range * distT);
        gain = div > 0.0f ? (cf.nearDist * (1.0f - distT) / div) : 1.0f;
      }

      if(dist > MIN_SOURCE_DIST)
      {
        pan = fm_vec3_dot(&to, &listener.right) / dist;
        // ease to the center inside nearDist
        // otherwise it flips L/R inside which sounds bad
        if(dist < cf.nearDist)pan *= dist / cf.nearDist;
      }
    }

    if(e.ch < 0)return;

    float vol = e.volume * gain * P64::AudioManager::masterVol;
    mixer_ch_set_vol(e.ch, vol * fminf(1.0f, 1.0f - pan), vol * fminf(1.0f, 1.0f + pan));
    mixer_ch_set_freq(e.ch, e.audio->wave.frequency * e.speed);
  }
}

namespace P64::AudioManager
{
  constinit float masterVol{1.0f};
  constinit uint64_t ticksUpdate{0};
  constinit int lastFreq{0};

  void setMasterVolume(float volume) {
    masterVol = volume;
  }

  void init(int freq)
  {
    if(freq != lastFreq)
    {
      if(lastFreq != 0)
      {
        Log::info("Audio freq. changed: %d -> %d", lastFreq, freq);
        stopAll();
        mixer_close();
        audio_close();
      }

      audio_init(freq, 3);
      mixer_init(CHANNEL_COUNT);
      slots = {};
      emitters = {};
      lastFreq = freq;
    }
  }

  void update()
  {
    auto ticks = get_ticks();
    mixer_try_play();
    for(uint32_t i=0; i<CHANNEL_COUNT; ++i)
    {
      if(!slots[i].hasAudio())continue;

      bool isPlaying = slots[i].isXM
        ? slots[i].audioXM->playing
        : mixer_ch_playing((int)i);

      uint16_t uuid = slots[i].uuid;
      if(isPlaying)
      {
        // apply master volume, 3D sounds handle it in update3D()
        if(slots[i].emitter < 0)updateVolume(i);
        while(i < CHANNEL_COUNT && slots[i].uuid == uuid)++i;
        --i;
      } else {
        // sound is stopped, free up slots again
        if(slots[i].emitter >= 0)emitters[slots[i].emitter] = {};
        for(; i < CHANNEL_COUNT && slots[i].uuid == uuid; ++i) {
          slots[i].clear();
        }
        --i;
      }
    }
    ticksUpdate += get_ticks() - ticks;
  }

  void update3D(float deltaTime)
  {
    auto ticks = get_ticks();
    resolveListenerSource();

    for(uint32_t i=0; i<EMITTER_COUNT; ++i)
    {
      auto &e = emitters[i];
      if(!e.isActive())continue;

      if(e.ch < 0)e.time += deltaTime * e.speed;

      fm_vec3_t to = e.pos - listener.pos;
      if(fm_vec3_len2(&to) >= (e.conf.maxDist * e.conf.maxDist))
      {
        e.outOfRangeTime += deltaTime;
        if(e.outOfRangeTime > CULL_TIME) {
          if(!e.loop) {
            freeEmitter(e);
            continue;
          }
          releaseEmitterChannel(e);
        }
      } else {
        e.outOfRangeTime = 0.0f;
        if(e.ch < 0)startEmitterChannel(e, i); // no free channel: try again next frame
      }
      updateEmitterChannel(e);
    }
    ticksUpdate += get_ticks() - ticks;
  }

  void destroy() {
    stopAll();
    mixer_close();
    audio_close();
  }

  Metrics getMetrics()
  {
    Metrics res{};
    for(uint32_t i=0; i<CHANNEL_COUNT; ++i)
    {
      if(slots[i].hasAudio())res.maskAlloc |= (1 << i);
      if(mixer_ch_playing(i)) {
        res.maskPlaying |= (1 << i);
      }
    }
    return res;
  }

  Audio::Handle play2D(wav64_t *audio, bool loop) {
    auto slot = getFreeSlots(audio->wave.channels);
    if(slot < 0) {
      Log::warn("No free audio channels left! needs: %d", audio->wave.channels);
      return {};
    }

    ++nextUUID;
    for(int s=slot; s < slot + audio->wave.channels; ++s) {
      slots[s].audioWAV = audio;
      slots[s].uuid = nextUUID;
      slots[s].volume = 1.0f;
      slots[s].isXM = 0;
    }

    playWav(audio, slot, loop);
    //Log::info("Playing audio on channel %d, uuid: %d", slot, nextUUID);
    return Audio::Handle{(uint16_t)slot, nextUUID};
  }

  Audio::Handle play2D(xm64player_t* audio)
  {
    int channels = xm64player_num_channels(audio);
    auto slot = getFreeSlots(channels);
    if(slot < 0) {
      Log::warn("No free audio channels left! needs: %d", channels);
      return {};
    }

    ++nextUUID;
    for(int s=slot; s < slot + channels; ++s) {
      slots[s].audioXM = audio;
      slots[s].uuid = nextUUID;
      slots[s].volume = 1.0f;
      slots[s].isXM = 1;
    }

    xm64player_play(audio, slot);
    return Audio::Handle{(uint16_t)slot, nextUUID};
  }

  Audio::Handle play3D(wav64_t *audio, const fm_vec3_t &pos, const Audio::Conf3D &conf, bool loop)
  {
    for(uint32_t i=0; i<EMITTER_COUNT; ++i)
    {
      auto &e = emitters[i];
      if(e.isActive())continue;

      ++nextUUID;
      e = Emitter{
        .audio = audio,
        .conf = conf,
        .pos = pos,
        .loop = loop,
        .uuid = nextUUID,
      };

      fm_vec3_t to = pos - listener.pos;
      if(fm_vec3_len2(&to) < (conf.maxDist * conf.maxDist)) {
        if(!startEmitterChannel(e, i) && !loop) {
          Log::warn("No free audio channels left! needs: %d", audio->wave.channels);
          e = Emitter{};
          return {};
        }
      } else if(!loop) {
        e = Emitter{};
        return {};
      }

      updateEmitterChannel(e);
      return Audio::Handle::create3D(i, nextUUID);
    }

    Log::warn("No free 3D audio emitters left!");
    return {};
  }

  void setListener(const fm_vec3_t &pos, const fm_vec3_t &forward, const fm_vec3_t &up) {
    listener.pos = pos;
    fm_vec3_cross(&listener.right, &forward, &up);
    fm_vec3_norm(&listener.right, &listener.right);
  }

  void setListenerSource(const ListenerSource &source) {
    listenerSource = source;
    hasListenerSource = true;
  }

  void stopAll() {
    for(uint32_t i=0; i<CHANNEL_COUNT; i++)mixer_ch_stop(i);
    slots = {};
    emitters = {};
  }
}

namespace
{
  Emitter* getEmitter(uint16_t slot, uint16_t uuid) {
    auto &e = emitters[slot & (EMITTER_COUNT-1)];
    return (e.isActive() && e.uuid == uuid) ? &e : nullptr;
  }
}

void P64::Audio::Handle::stop() {
  if(is3D()) {
    if(auto e = getEmitter(slot, uuid))freeEmitter(*e);
    uuid = 0;
    return;
  }

  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;

  if(entry->isXM) {
    auto chCount = xm64player_num_channels(entry->audioXM);
    for(int s=slot; s < slot + chCount; ++s) {
      slots[s].clear();
    }
    xm64player_stop(entry->audioXM);
    return;
  }

  mixer_ch_stop(slot);
  uuid = 0;
}

void P64::Audio::Handle::setVolume(float volume)
{
  if(is3D()) {
    if(auto e = getEmitter(slot, uuid))e->volume = volume;
    return;
  }

  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;
  entry->volume = volume;
  updateVolume(slot);
}

void P64::Audio::Handle::setSpeed(float speed)
{
  if(is3D()) {
    if(auto e = getEmitter(slot, uuid))e->speed = speed;
    return;
  }

  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;

  if(entry->isXM)
  {
    Log::warn("setSpeed is not supported for XM audio! uuid: %d", uuid);
    return;
  }

  entry->speed = speed;
  float freq = entry->audioWAV->wave.frequency * speed;
  mixer_ch_set_freq(slot, freq);
}

bool P64::Audio::Handle::isDone() {
  if(is3D())return getEmitter(slot, uuid) == nullptr;

  auto entry = &slots[slot];
  if(entry->uuid != uuid)return true;

  if(entry->isXM)
  {
    return !entry->audioXM->playing;
  } else {
    if (entry->audioWAV == nullptr) return true;
    return !mixer_ch_playing(slot);
  }
}

void P64::Audio::Handle::setPosition(const fm_vec3_t &pos) {
  if(!is3D())return;
  if(auto e = getEmitter(slot, uuid))e->pos = pos;
}
