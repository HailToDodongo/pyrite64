/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::Assets::Type
{
  constexpr uint8_t UNKNOWN = 0;
  constexpr uint8_t IMAGE = 1;
  constexpr uint8_t AUDIO = 2;
  constexpr uint8_t FONT = 3;
  constexpr uint8_t MODEL_3D = 4;
  constexpr uint8_t CODE_OBJ = 5;
  constexpr uint8_t CODE_GLOBAL = 6;
  constexpr uint8_t PREFAB = 7;
  constexpr uint8_t NODE_GRAPH = 8;
}