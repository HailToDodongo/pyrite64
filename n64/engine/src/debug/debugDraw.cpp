/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "debug/debugDraw.h"
#include <t3d/t3d.h>
#include <vector>

namespace
{
  struct Line {
    fm_vec3_t a{};
    fm_vec3_t b{};
    uint16_t color;
    uint16_t _padding;
  };

  std::vector<Line> lines{};

  sprite_t *font{};

  void debugDrawLine(uint16_t *fb, int px0, int py0, int px1, int py1, uint16_t color)
  {
    int width = 320;
    int height = 240;
    if((px0 > width + 200) || (px1 > width + 200) ||
       (py0 > height + 200) || (py1 > height + 200)) {
      return;
    }

    float pos[2]{(float)px0, (float)py0};
    int dx = px1 - px0;
    int dy = py1 - py0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if(steps <= 0)return;
    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    for(int i=0; i<steps; ++i)
    {
      if((i%3 != 0) && pos[1] >= 0 && pos[1] < height && pos[0] >= 0 && pos[0] < width) {
        fb[(int)pos[1] * width + (int)pos[0]] = color;
      }
      pos[0] += xInc;
      pos[1] += yInc;
    }
  }
}

void Debug::init() {
  font = sprite_load("rom:/p64/font.ia4.sprite");
  lines = {};
}

void Debug::destroy() {
  lines.clear();
  lines.shrink_to_fit();
  sprite_free(font);
}

void Debug::drawLine(const fm_vec3_t &a, const fm_vec3_t &b, color_t color) {
  if(lines.size() > 1000)return;
  lines.push_back({a, b, color_to_packed16(color), 0});
}

void Debug::drawSphere(const fm_vec3_t &center, float radius, color_t color) {

  int steps = 12;
  float step = 2.0f * (float)M_PI / steps;
  fm_vec3_t last = center + fm_vec3_t{radius, 0, 0};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    fm_vec3_t next = center + fm_vec3_t{radius * fm_cosf(angle), 0, radius * fm_sinf(angle)};
    drawLine(last, next, color);
    last = next;
  }
  last = center + fm_vec3_t{0, radius, 0};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    fm_vec3_t next = center + fm_vec3_t{0, radius * fm_cosf(angle), radius * fm_sinf(angle)};
    drawLine(last, next, color);
    last = next;
  }
  last = center + fm_vec3_t{0, 0, radius};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    fm_vec3_t next = center + fm_vec3_t{radius * fm_cosf(angle), radius * fm_sinf(angle), 0};
    drawLine(last, next, color);
    last = next;
  }
}

void Debug::draw(uint16_t *fb) {
  if(lines.empty())return;
  debugf("Drawing %d lines, %d rects\n", lines.size());
  //rspq_wait();

  for(auto &line : lines) {
    t3d_viewport_calc_viewspace_pos(nullptr, &line.a, &line.a);
    t3d_viewport_calc_viewspace_pos(nullptr, &line.b, &line.b);
  }

  auto vp = t3d_viewport_get();
  float maxX = vp->size[0];
  float maxY = vp->size[1];
  for(auto &line : lines) {
    if(line.a.x < 0 && line.b.x < 0)continue;
    if(line.a.y < 0 && line.b.y < 0)continue;
    if(line.a.x > maxX && line.b.x > maxX)continue;
    if(line.a.y > maxY && line.b.y > maxY)continue;
    debugDrawLine(fb, line.a.x, line.a.y, line.b.x, line.b.y, line.color);
  }

  /*for(auto &rect : rects) {
    if(rect.min[0] < 0 || rect.min[1] < 0)continue;
    if(rect.max[0] > maxX || rect.max[1] > maxY)continue;
    debugDrawLine(fb, rect.min[0], rect.min[1], rect.max[0], rect.min[1], 0xF000);
    debugDrawLine(fb, rect.min[0], rect.min[1], rect.min[0], rect.max[1], 0xF000);
    debugDrawLine(fb, rect.max[0], rect.min[1], rect.max[0], rect.max[1], 0xF000);
    debugDrawLine(fb, rect.min[0], rect.max[1], rect.max[0], rect.max[1], 0xF000);
  }*/

  lines.clear();
  lines.shrink_to_fit();
  //rects.clear();
  //rects.shrink_to_fit();
}

void Debug::printStart() {
  rdpq_sync_pipe();
  rdpq_sync_tile();
  rdpq_sync_load();

  rdpq_set_mode_standard();
  rdpq_mode_antialias(AA_NONE);
  rdpq_mode_combiner(RDPQ_COMBINER1((TEX0,0,PRIM,0), (TEX0,0,PRIM,0)));
  rdpq_mode_alphacompare(1);
  rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));

  rdpq_sprite_upload(TILE0, font, NULL);
}

float Debug::print(float x, float y, const char *str) {
  int width = 8;
  int height = 8;
  int s = 0;

  while(*str) {
    uint8_t c = *str;
    if(c != ' ' && c != '\n')
    {
      if(c >= 'a' && c <= 'z')c &= ~0x20;
      s = (c - 33) * width;
      rdpq_texture_rectangle_raw(TILE0, x, y, x+width, y+height, s, 0, 1, 1);
    }
    ++str;
    x += 7;
  }
  return x;
}

float Debug::printf(float x, float y, const char *fmt, ...) {
  if(x > 320-8)return x;
  char buffer[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, 128, fmt, args);
  va_end(args);
  return Debug::print(x, y, buffer);
}

void Debug::drawAABB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, color_t color) {
  fm_vec3_t a = p - halfExtend;
  fm_vec3_t b = p + halfExtend;
  // draw all 12 edges
  drawLine(a, fm_vec3_t{b.x, a.y, a.z}, color);
  drawLine(a, fm_vec3_t{a.x, b.y, a.z}, color);
  drawLine(a, fm_vec3_t{a.x, a.y, b.z}, color);
  drawLine(fm_vec3_t{b.x, a.y, a.z}, fm_vec3_t{b.x, b.y, a.z}, color);
  drawLine(fm_vec3_t{b.x, a.y, a.z}, fm_vec3_t{b.x, a.y, b.z}, color);
  drawLine(fm_vec3_t{a.x, b.y, a.z}, fm_vec3_t{b.x, b.y, a.z}, color);
  drawLine(fm_vec3_t{a.x, b.y, a.z}, fm_vec3_t{a.x, b.y, b.z}, color);
  drawLine(fm_vec3_t{a.x, a.y, b.z}, fm_vec3_t{b.x, a.y, b.z}, color);
  drawLine(fm_vec3_t{a.x, a.y, b.z}, fm_vec3_t{a.x, b.y, b.z}, color);
  drawLine(fm_vec3_t{b.x, b.y, a.z}, fm_vec3_t{b.x, b.y, b.z}, color);
  drawLine(fm_vec3_t{b.x, b.y, a.z}, fm_vec3_t{a.x, b.y, a.z}, color);
  drawLine(fm_vec3_t{b.x, b.y, b.z}, fm_vec3_t{a.x, b.y, b.z}, color);
}
