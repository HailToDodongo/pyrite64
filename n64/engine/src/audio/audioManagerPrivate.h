/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once

namespace P64::AudioManager {
  extern float masterVol;

  void init(int freq = 32000);
  void update();
  void destroy();

  struct Metrics
  {
    uint32_t maskPlaying;
    uint32_t maskAlloc;
  };
  Metrics getMetrics();
}
