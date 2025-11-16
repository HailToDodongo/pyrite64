/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace Debug
{
  void init();

  void drawAABB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, color_t color = {0xFF, 0xFF, 0xFF, 0xFF});
  void drawLine(const fm_vec3_t &a, const fm_vec3_t &b, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawSphere(const fm_vec3_t &center, float radius, color_t color = {0xFF,0xFF,0xFF,0xFF});

  void draw(uint16_t *fb);

  void printStart();
  float print(float x, float y, const char* str);
  float printf(float x, float y, const char *fmt, ...);

  void destroy();
}