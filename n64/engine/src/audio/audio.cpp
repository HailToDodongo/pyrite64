/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "audio/audio.h"
#include "audioPrivate.h"
#include "lib/logger.h"

#include <libdragon.h>

namespace
{
  // samples whose stored data exceeds this stream from ROM instead of staying resident
  constexpr uint32_t STREAM_THRESHOLD = 8 * 1024;
  constexpr uint32_t STREAM_WINDOW = 8 * 1024;
  constexpr uint32_t STREAM_BUDGET = 256 * 1024;
  constexpr int NUM_BUFFERS = 4;

  TSQ::SoundEngine soundEngine{};
}

namespace P64::Audio
{
  constinit TSQ::LivePlayer *sfx{nullptr};
  constinit uint64_t ticksUpdate{0};

  // requested rate, the engine reports the hardware-adjusted one (e.g. 32000 -> 31995),
  // so comparing against that would re-init on every scene load
  constinit int currentFreq{0};

  void init(int freq)
  {
    if(freq <= 0)freq = 32000;

    if(soundEngine.isReady())
    {
      if(currentFreq == freq) {
        if(!sfx)sfx = soundEngine.createLivePlayer();
        return;
      }
      Log::info("Audio freq. changed: %d -> %d", currentFreq, freq);
      soundEngine.destroyAllPlayers();
      sfx = nullptr;
      soundEngine.close();
    }

    currentFreq = freq;
    soundEngine.init(freq, NUM_BUFFERS);
    soundEngine.initStreaming(STREAM_WINDOW, STREAM_BUDGET);
    soundEngine.setStreamThreshold(STREAM_THRESHOLD);
    sfx = soundEngine.createLivePlayer();
  }

  void update()
  {
    auto ticks = get_ticks();
    soundEngine.update();
    ticksUpdate += get_ticks() - ticks;
  }

  void sceneReset()
  {
    soundEngine.stopAll();
    soundEngine.destroyAllPlayers();
    sfx = nullptr;
  }
}
