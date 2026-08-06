/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

namespace P64::Audio
{
  extern uint64_t ticksUpdate;

  void init(int freq = 32000);
  void update();
  void sceneReset();
}
