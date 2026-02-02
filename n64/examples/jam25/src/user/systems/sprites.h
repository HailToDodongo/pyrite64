/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <renderer/particles/ptxSprites.h>

namespace P64::User::Sprites
{
  inline PTX::Sprites* coin;
  inline PTX::Sprites* coinPart;
  inline PTX::Sprites* dust;

  void init();
  void destroy();

  void reset();
  void draw();
}
