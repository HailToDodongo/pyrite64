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
  constinit float masterVol{1.0f};

  struct Slot
  {
    wav64_t* audio{nullptr};
    float volume{1.0f};
    float speed{1.0f};
    uint16_t uuid{0};
  };

  std::array<Slot, CHANNEL_COUNT> slots{};

  int32_t getFreeSlot() {
    for(uint32_t i=0; i<slots.size(); ++i) {
      if(!slots[i].audio)return (int32_t)i;
    }
    return -1;
  }

  int32_t getFreeSlotStereo() {
    for(uint32_t i=0; i<slots.size()-1; ++i) {
      if(!slots[i].audio && !slots[i+1].audio)return (int32_t)i;
    }
    return -1;
  }
}

namespace P64::AudioManager
{
  constinit uint64_t ticksUpdate{0};

  void setMasterVolume(float volume) {
    masterVol = volume;
  }

  void init(int freq) {
    audio_init(freq, 3);
    mixer_init(CHANNEL_COUNT);
    slots = {};
  }

  void update()
  {
    auto ticks = get_ticks();
    mixer_try_play();
    for(uint32_t i=0; i<CHANNEL_COUNT; ++i) {
      if(slots[i].audio && !mixer_ch_playing((int)i))
      {
        if(slots[i].audio == slots[i+1].audio) { // stereo
          slots[i+1].audio = nullptr;
        }
        slots[i].audio = nullptr;
      }

      if (slots[i].audio) { // mono

        if (slots[i].audio->wave.channels == 2) {
          mixer_ch_set_vol(i, slots[i].volume * masterVol, slots[i].volume * masterVol);
          ++i;
        }
      }
    }
    ticksUpdate += get_ticks() - ticks;
  }

  void destroy() {
    stopAll();
    mixer_close();
    audio_close();
  }

  Audio::Handle play2D(wav64_t *audio) {
    auto slot = audio->wave.channels == 2 ? getFreeSlotStereo() : getFreeSlot();
    if(slot < 0)return {};

    ++nextUUID;

    slots[slot].audio = audio;
    slots[slot].uuid = nextUUID;
    slots[slot].volume = 1.0f;

    if(audio->wave.channels == 2) {
      slots[slot+1] = slots[slot];
    }

    wav64_play(audio, slot);
    //Log::info("Playing audio on channel %d, uuid: %d", slot, nextUUID);
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
  mixer_ch_stop(slot);
  uuid = 0;
}

void P64::Audio::Handle::setVolume(float volume)
{
  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;

  entry->volume = volume;

  // hack:
  if (entry->audio->wave.channels == 2) {
    volume *= masterVol;
  }

  mixer_ch_set_vol(slot, volume, volume);
}

void P64::Audio::Handle::setSpeed(float speed)
{
  auto entry = &slots[slot];
  if(entry->uuid != uuid)return;
  entry->speed = speed;
  float freq = entry->audio->wave.frequency * speed;
  mixer_ch_set_freq(slot, freq);
}

bool P64::Audio::Handle::isDone() {
  auto entry = &slots[slot];
  if(entry->uuid != uuid)return true;
  if (entry->audio == nullptr) return true;
  return !mixer_ch_playing(slot);
}
