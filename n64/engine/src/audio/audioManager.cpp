/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "audio/audioManager.h"
#include "lib/logger.h"
#include "audioManagerPrivate.h"

#include <libdragon.h>
#include <array>

namespace
{
  constexpr uint32_t CHANNEL_COUNT = 32;
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
    uint8_t isXM{0}; // (turn into flags once more settings are needed)

    [[nodiscard]] bool hasAudio() const { return audioWAV != nullptr || audioXM != nullptr; }
    void clear() { *this = Slot{}; }
  };

  std::array<Slot, CHANNEL_COUNT> slots{};

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

      if(isPlaying)
      {
        // apply master volume (@TODO: implement and handle 3D sound / panning)
        updateVolume(i);
        uint16_t uuid = slots[i].uuid;
        while(i < CHANNEL_COUNT && slots[i].uuid == uuid)++i;
        --i;
      } else {
        // sound is stopped, free up slots again
        uint16_t uuid = slots[i].uuid;
        for(; i < CHANNEL_COUNT && slots[i].uuid == uuid; ++i) {
          slots[i].clear();
        }
        --i;
      }
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

  Audio::Handle play2D(wav64_t *audio) {
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

    wav64_play(audio, slot);
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

  void stopAll() {
    for(uint32_t i=0; i<CHANNEL_COUNT; i++)mixer_ch_stop(i);
    slots = {};
  }
}

void P64::Audio::Handle::stop() {
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
  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;
  entry->volume = volume;
  updateVolume(slot);
}

void P64::Audio::Handle::setSpeed(float speed)
{
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
