/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../overlay.h"
#include "../../audio/audioPrivate.h"
#include "audio/audio.h"
#include "debug/debugDraw.h"

constexpr uint32_t SCREEN_HEIGHT = 240;

void P64::Debug::Overlay::ovlAudio()
{
  uint16_t posX = 24;
  uint16_t posY = SCREEN_HEIGHT - 62;

  auto &engine = P64::Audio::engine();
  const auto &metrics = engine.getMetrics();

  P64::Debug::isMonospace = true;
  P64::Debug::printf(posX, posY, "Voices  %d", metrics.activeVoices);
  posY += 8;
  P64::Debug::printf(posX, posY, "Streams %d/%d (miss %lu)",
    metrics.streamInUse, metrics.streamSlots, metrics.streamMisses);
  posY += 8;
  P64::Debug::printf(posX, posY, "Steals  %lu/%lu/%lu",
    metrics.stealsActive, metrics.stealsReleasing, metrics.stealsDropped);
  posY += 8;
  P64::Debug::printf(posX, posY, "Time    seq:%luus rsp:%luus",
    metrics.times.seq, metrics.times.rsp);
  P64::Debug::isMonospace = false;
}
