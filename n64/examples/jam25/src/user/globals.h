/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::User
{
  constexpr uint8_t FONT_TITLE = 2;
  constexpr uint8_t FONT_SMALL = 3;
  constexpr uint8_t FONT_TEXT  = 4;

  constexpr uint8_t COLL_LAYER_PLAYER = 1 << 0;
  constexpr uint8_t COLL_LAYER_HURT   = 1 << 1;

  color_t getRainbowColor(float s);
}
