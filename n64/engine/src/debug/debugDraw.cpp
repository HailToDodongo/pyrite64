/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "debug/debugDraw.h"
#include "collision/vec_math.h"
#include <t3d/t3d.h>
#include <cmath>
#include <vector>

namespace
{
  constexpr uint32_t MAX_LINE_COUNT = 4048;
  constexpr int SIMPLE_RING_STEPS = 8;

  inline fm_vec3_t toWorldPoint(const fm_vec3_t &position, const fm_quat_t &rotation, const fm_vec3_t &local)
  {
    return position + rotation * local;
  }

  void drawRingXZ(const fm_vec3_t &position, const fm_quat_t &rotation, float radius, float y, int steps, color_t color)
  {
    float step = 2.0f * (float)M_PI / steps;
    fm_vec3_t last{radius, y, 0.0f};
    for(int i = 1; i <= steps; ++i) {
      float angle = i * step;
      fm_vec3_t next{radius * fm_cosf(angle), y, radius * fm_sinf(angle)};
      P64::Debug::drawLine(toWorldPoint(position, rotation, last), toWorldPoint(position, rotation, next), color);
      last = next;
    }
  }

  struct Line {
    fm_vec3_t a{};
    fm_vec3_t b{};
    uint16_t color;
    uint16_t _padding;
  };

  std::vector<Line> lines{};

  void debugDrawLine(surface_t *fb, int px0, int py0, int px1, int py1, uint16_t color)
  {
    int width = fb->width;
    int height = fb->height;
    if((px0 > width + 200) || (px1 > width + 200) ||
       (py0 > height + 200) || (py1 > height + 200)) {
      return;
    }

    float pos[2]{(float)px0, (float)py0};
    int dx = px1 - px0;
    int dy = py1 - py0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if(steps <= 0 || steps > 1000)return;
    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    if(surface_get_format(fb) == FMT_RGBA16)
    {
      uint16_t *buff = (uint16_t*)fb->buffer;
      for(int i=0; i<steps; ++i)
      {
        if((i%3 != 0) && pos[1] >= 0 && pos[1] < height && pos[0] >= 0 && pos[0] < width) {
          buff[(int)pos[1] * width + (int)pos[0]] = color;
        }
        pos[0] += xInc;
        pos[1] += yInc;
      }
    } else
    {
      uint32_t col32 = color_to_packed32(color_from_packed16(color));
      uint32_t *buff = (uint32_t*)fb->buffer;
      for(int i=0; i<steps; ++i)
      {
        if((i%3 != 0) && pos[1] >= 0 && pos[1] < height && pos[0] >= 0 && pos[0] < width) {
          buff[(int)pos[1] * width + (int)pos[0]] = col32;
        }
        pos[0] += xInc;
        pos[1] += yInc;
      }
    }
  }
}

void P64::Debug::init() {
  lines = {};
}

void P64::Debug::destroy() {
  lines = {};
}

void P64::Debug::drawLine(const fm_vec3_t &a, const fm_vec3_t &b, color_t color) {
  if(lines.size() > MAX_LINE_COUNT)return;
  lines.push_back({a, b, color_to_packed16(color), 0});
}

color_t P64::Debug::paletteColor(uint32_t index) {
  static constexpr color_t PALETTE[] = {
    {0xFF, 0x66, 0x66, 0xFF},
    {0x66, 0xFF, 0x99, 0xFF},
    {0x66, 0xB3, 0xFF, 0xFF},
    {0xFF, 0xCC, 0x66, 0xFF},
    {0xC2, 0x7C, 0xFF, 0xFF},
    {0x66, 0xE6, 0xE6, 0xFF}
  };
  constexpr uint32_t COUNT = sizeof(PALETTE) / sizeof(PALETTE[0]);
  return PALETTE[index % COUNT];
}

void P64::Debug::drawSphere(const fm_vec3_t &center, float radius, color_t color) {

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

void P64::Debug::draw(surface_t *fb) {
  if(lines.empty())return;

  // debugf("Drawing %u lines\n", lines.size());
  rspq_wait();

  auto vp = t3d_viewport_get();
  float maxX = vp->size[0];
  float maxY = vp->size[1];
  for(auto &line : lines) {
    t3d_viewport_calc_viewspace_pos(nullptr, &line.a, &line.a);
    t3d_viewport_calc_viewspace_pos(nullptr, &line.b, &line.b);

    if(line.a.z > 1)continue;
    if(line.b.z > 1)continue;

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

void P64::Debug::drawAABB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, color_t color) {
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

void P64::Debug::drawOBB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, const fm_quat_t &rot, color_t color) {
  fm_vec3_t a = toWorldPoint(p, rot, -halfExtend); // left bottom front
  fm_vec3_t b = toWorldPoint(p, rot, {halfExtend.x, -halfExtend.y, -halfExtend.z}); // right bottom front
  fm_vec3_t c = toWorldPoint(p, rot, {halfExtend.x, -halfExtend.y, halfExtend.z}); // right bottom back
  fm_vec3_t d = toWorldPoint(p, rot, {-halfExtend.x, -halfExtend.y, halfExtend.z}); // left bottom back
  fm_vec3_t e = toWorldPoint(p, rot, {-halfExtend.x, halfExtend.y, -halfExtend.z}); // left top front
  fm_vec3_t f = toWorldPoint(p, rot, {halfExtend.x, halfExtend.y, -halfExtend.z}); // right top front
  fm_vec3_t g = toWorldPoint(p, rot, halfExtend); // right top back
  fm_vec3_t h = toWorldPoint(p, rot, {-halfExtend.x, halfExtend.y, halfExtend.z}); // left top back
  
  // draw all 12 edges
  drawLine(a, b, color);
  drawLine(b, c, color);
  drawLine(c, d, color);
  drawLine(d, a, color);
  drawLine(e, f, color);
  drawLine(f, g, color);
  drawLine(g, h, color);
  drawLine(a, e, color);
  drawLine(e, h, color);
  drawLine(b, f, color);
  drawLine(c, g, color);
  drawLine(d, h, color);
}

void P64::Debug::drawCapsule(const fm_vec3_t &p, float radius, float innerHalfHeight, const fm_quat_t &rot, color_t color) {
  constexpr int ARC_STEPS = 6;

  auto drawHemisphereArcs = [&](float centerY, float signY) {
    float step = (float)M_PI / ARC_STEPS;

    fm_vec3_t lastXY{radius, centerY, 0.0f};
    fm_vec3_t lastZY{0.0f, centerY, radius};

    for(int i = 1; i <= ARC_STEPS; ++i) {
      float angle = i * step;
      float s = fm_sinf(angle);
      float c = fm_cosf(angle);

      fm_vec3_t nextXY{radius * c, centerY + signY * radius * s, 0.0f};
      fm_vec3_t nextZY{0.0f, centerY + signY * radius * s, radius * c};

      drawLine(toWorldPoint(p, rot, lastXY), toWorldPoint(p, rot, nextXY), color);
      drawLine(toWorldPoint(p, rot, lastZY), toWorldPoint(p, rot, nextZY), color);

      lastXY = nextXY;
      lastZY = nextZY;
    }
  };

  float topY = innerHalfHeight;
  float bottomY = -innerHalfHeight;

  drawRingXZ(p, rot, radius, topY, SIMPLE_RING_STEPS, color);
  drawRingXZ(p, rot, radius, bottomY, SIMPLE_RING_STEPS, color);
  drawHemisphereArcs(topY, 1.0f);
  drawHemisphereArcs(bottomY, -1.0f);

  drawLine(toWorldPoint(p, rot, {radius, topY, 0.0f}), toWorldPoint(p, rot, {radius, bottomY, 0.0f}), color);
  drawLine(toWorldPoint(p, rot, {-radius, topY, 0.0f}), toWorldPoint(p, rot, {-radius, bottomY, 0.0f}), color);
  drawLine(toWorldPoint(p, rot, {0.0f, topY, radius}), toWorldPoint(p, rot, {0.0f, bottomY, radius}), color);
  drawLine(toWorldPoint(p, rot, {0.0f, topY, -radius}), toWorldPoint(p, rot, {0.0f, bottomY, -radius}), color);
}

void P64::Debug::drawCylinder(const fm_vec3_t &p, float radius, float halfHeight, const fm_quat_t &rot, color_t color) {
  drawRingXZ(p, rot, radius, halfHeight, SIMPLE_RING_STEPS, color);
  drawRingXZ(p, rot, radius, -halfHeight, SIMPLE_RING_STEPS, color);

  drawLine(toWorldPoint(p, rot, {radius, halfHeight, 0.0f}), toWorldPoint(p, rot, {radius, -halfHeight, 0.0f}), color);
  drawLine(toWorldPoint(p, rot, {-radius, halfHeight, 0.0f}), toWorldPoint(p, rot, {-radius, -halfHeight, 0.0f}), color);
  drawLine(toWorldPoint(p, rot, {0.0f, halfHeight, radius}), toWorldPoint(p, rot, {0.0f, -halfHeight, radius}), color);
  drawLine(toWorldPoint(p, rot, {0.0f, halfHeight, -radius}), toWorldPoint(p, rot, {0.0f, -halfHeight, -radius}), color);
}

void P64::Debug::drawCone(const fm_vec3_t &p, float radius, float halfHeight, const fm_quat_t &rot, color_t color) {
  drawRingXZ(p, rot, radius, -halfHeight, SIMPLE_RING_STEPS, color);

  fm_vec3_t apex = toWorldPoint(p, rot, {0.0f, halfHeight, 0.0f});
  drawLine(apex, toWorldPoint(p, rot, {radius, -halfHeight, 0.0f}), color);
  drawLine(apex, toWorldPoint(p, rot, {-radius, -halfHeight, 0.0f}), color);
  drawLine(apex, toWorldPoint(p, rot, {0.0f, -halfHeight, radius}), color);
  drawLine(apex, toWorldPoint(p, rot, {0.0f, -halfHeight, -radius}), color);
}

void P64::Debug::drawPyramid(const fm_vec3_t &p, float baseHalfWidthX, float baseHalfWidthZ, float halfHeight, const fm_quat_t &rot, color_t color) {
  fm_vec3_t a = toWorldPoint(p, rot, {-baseHalfWidthX, -halfHeight, -baseHalfWidthZ});
  fm_vec3_t b = toWorldPoint(p, rot, { baseHalfWidthX, -halfHeight, -baseHalfWidthZ});
  fm_vec3_t c = toWorldPoint(p, rot, { baseHalfWidthX, -halfHeight,  baseHalfWidthZ});
  fm_vec3_t d = toWorldPoint(p, rot, {-baseHalfWidthX, -halfHeight,  baseHalfWidthZ});
  fm_vec3_t apex = toWorldPoint(p, rot, {0.0f, halfHeight, 0.0f});

  drawLine(a, b, color);
  drawLine(b, c, color);
  drawLine(c, d, color);
  drawLine(d, a, color);

  drawLine(a, apex, color);
  drawLine(b, apex, color);
  drawLine(c, apex, color);
  drawLine(d, apex, color);
}
