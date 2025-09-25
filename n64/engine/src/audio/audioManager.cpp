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

  struct Slot
  {
    wav64_t* audio{nullptr};
  };

  std::array<Slot, CHANNEL_COUNT> slots{};
  uint16_t currentUUID{0};

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
  void init() {
    audio_init(32000, 3);
    mixer_init(CHANNEL_COUNT);
    slots = {};
  }

  void update()
  {
    mixer_try_play();
    for(uint32_t i=0; i<CHANNEL_COUNT; ++i) {
      if(slots[i].audio && !mixer_ch_playing((int)i)) {
        slots[i].audio = nullptr;
      }
    }
  }

  void destroy() {
    stopAll();
    mixer_close();
  }

  Audio::Handle play2D(wav64_t *audio) {
    auto slot = audio->wave.channels == 2 ? getFreeSlotStereo() : getFreeSlot();
    if(slot < 0)return {};

    slots[slot].audio = audio;
    wav64_play(audio, slot);
    Log::info("Playing audio on channel %d", slot);
    return Audio::Handle{0,0};
  }

  void stopAll() {
    for(uint32_t i=0; i<CHANNEL_COUNT; i++)mixer_ch_stop(i);
    slots = {};
  }
}

void P64::Audio::Handle::stop() {

}
