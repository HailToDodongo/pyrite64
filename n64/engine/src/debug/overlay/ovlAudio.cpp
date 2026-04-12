/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../overlay.h"
#include "../../audio/audioManagerPrivate.h"
#include "audio/audioManager.h"
#include "debug/debugDraw.h"

constexpr uint32_t SCREEN_HEIGHT = 240;
constexpr uint32_t SCREEN_WIDTH = 320;

void P64::Debug::Overlay::ovlAudio()
{
  uint16_t posX = 24;
  uint16_t posY = SCREEN_HEIGHT - 38;

  P64::Debug::isMonospace = true;
  posX = P64::Debug::printf(posX, posY, "Channel ");
  {
    auto audioMetrics = P64::AudioManager::getMetrics();
    char strMask[33] = {};
    strMask[32] = '\0';
    for(uint32_t i=0; i<32; ++i) {
      bool isPlaying = audioMetrics.maskPlaying & (1 << i);
      bool isUsed    = audioMetrics.maskAlloc & (1 << i);

      if(isPlaying && isUsed)strMask[i] = DEBUG_CHAR_SQUARE[0];
      else if(isUsed)strMask[i] = '-';
      else if(isPlaying)strMask[i] = '?';
      else strMask[i] = '.';
    }
    P64::Debug::print(posX, posY, strMask);
  }
  P64::Debug::isMonospace = false;
}
