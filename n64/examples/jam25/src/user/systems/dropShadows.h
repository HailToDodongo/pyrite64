/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::User::DropShadows
{
  void init();
  void destroy();

  void addShadow(const fm_vec3_t &pos, const fm_vec3_t &normal, float size, float strength);
  void draw();
  void reset();
}
