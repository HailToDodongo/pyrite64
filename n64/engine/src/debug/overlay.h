/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::Debug::Overlay
{
  extern uint64_t ticksSelf;
  extern bool useCpuAvg;

  void init();
  void draw(surface_t* surf);

  // sub overlays:
  void ovlAudio();
  void ovlMemory();
  void ovlCPU();
}
