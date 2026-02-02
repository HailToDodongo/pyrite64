/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::User::Marker
{
  struct Marker
  {
    int16_t pos[2]{};
    int16_t halfSize[2]{};
    uint16_t objectId{0};
  };

  void init();
  void nextFrame();
  void draw();

  void addMarker(const Marker &marker);

  uint16_t getClosestObjectId();
}