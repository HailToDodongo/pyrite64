/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

namespace P64::AudioManager {
  extern float masterVol;

  void init(int freq = 32000);
  void update();
  void update3D(float deltaTime);
  void destroy();

  struct ListenerSource
  {
    uint16_t posObjId{};
    uint16_t rotObjId{};
  };
  void setListenerSource(const ListenerSource &source);

  struct Metrics
  {
    uint32_t maskPlaying;
    uint32_t maskAlloc;
  };
  Metrics getMetrics();
}
