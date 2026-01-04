/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

// libdragon overloads:
static inline float fm_vec2_dot(const fm_vec2_t &a, const fm_vec2_t &b) {
  return ::fm_vec2_dot(&a, &b);
}

namespace P64::Math
{
  constexpr float SQRT_2_INV = 0.70710678118f;
  constexpr float PI = 3.14159265358979f;

  constexpr uint32_t alignUp(uint32_t val, uint32_t alignTo) {
    return (val + (alignTo - 1)) & ~(alignTo - 1);
  }
  static_assert(alignUp(1, 8) == 8);
  static_assert(alignUp(8, 8) == 8);
  static_assert(alignUp(9, 8) == 16);

  constexpr uint32_t alignDown(uint32_t val, uint32_t alignTo) {
    return val & ~(alignTo - 1);
  }
  static_assert(alignDown(1, 8) == 0);
  static_assert(alignDown(8, 0) == 0);
  static_assert(alignDown(9, 8) == 8);


  constexpr float s10ToFloat(uint32_t value, float offset, float scale) {
    return (float)value / 1023.0f * scale + offset;
  }

  inline fm_quat_t unpackQuat(uint32_t quatPacked)
  {
    constexpr int BITS = 10;
    constexpr int BIT_MASK = (1 << BITS) - 1;

    uint32_t largestIdx = quatPacked >> 30;
    uint32_t idx0 = (largestIdx + 1) & 0b11;
    uint32_t idx1 = (largestIdx + 2) & 0b11;
    uint32_t idx2 = (largestIdx + 3) & 0b11;

    float q0 = s10ToFloat((quatPacked >> (BITS * 2)) & BIT_MASK, -SQRT_2_INV, SQRT_2_INV*2);
    float q1 = s10ToFloat((quatPacked >> (BITS * 1)) & BIT_MASK, -SQRT_2_INV, SQRT_2_INV*2);
    float q2 = s10ToFloat((quatPacked >> (BITS * 0)) & BIT_MASK, -SQRT_2_INV, SQRT_2_INV*2);

    fm_quat_t q{};
    q.v[idx0] = q0;
    q.v[idx1] = q1;
    q.v[idx2] = q2;
    q.v[largestIdx] = sqrtf(1.0f - q0*q0 - q1*q1 - q2*q2);
    return q;
  }

  inline float easeOutCubic(float x) {
    x = 1.0f - x;
    return 1.0f - (x*x*x);
  }

  inline float easeInOutCubic(float x) {
    x *= 2.0f;
    if(x < 1.0f) {
      return 0.5f * x * x * x;
    }
    x -= 2.0f;
    return 0.5f * (x * x * x + 2.0f);
  }

  inline float easeOutSin(float x) {
    return fm_sinf((x * PI) * 0.5f);
  }

  inline int noise2d(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (n * (n * n * 60493 + 19990303) + 89);
  }

  inline float rand01() {
    return (rand() % 4096) / 4096.0f;
  }

  inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
  }

  template<typename T>
  inline auto min(T a, T b) { return a < b ? a : b; }

  template<typename T>
  inline auto max(T a, T b) { return a > b ? a : b; }

  template<typename T>
  inline auto clamp(T val, T min, T max) {
    return val < min ? min : (val > max ? max : val);
  }

  inline auto min(const fm_vec3_t &a) {
    return fminf(a.x, fminf(a.y, a.z));
  }
  inline auto max(const fm_vec3_t &a) {
    return fmaxf(a.x, fmaxf(a.y, a.z));
  }

  inline auto min(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)}};
  }
  inline auto max(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)}};
  }

  inline auto abs(const fm_vec3_t &a) {
    return fm_vec3_t{{fabsf(a.x), fabsf(a.y), fabsf(a.z)}};
  }

  inline auto cross(const fm_vec3_t &a, const fm_vec3_t &b) {
    fm_vec3_t res;
    fm_vec3_cross(&res, &a, &b);
    return res;
  }

  inline fm_vec3_t sign(const fm_vec3_t &v) {
    return fm_vec3_t{{
      v.x < 0.0f ? -1.0f : (v.x > 0.0f ? 1.0f : 0.0f),
      v.y < 0.0f ? -1.0f : (v.y > 0.0f ? 1.0f : 0.0f),
      v.z < 0.0f ? -1.0f : (v.z > 0.0f ? 1.0f : 0.0f)
    }};
  }

  inline fm_vec3_t randDir3D() {
    fm_vec3_t res{{rand01()-0.5f, rand01()-0.5f, rand01()-0.5f}};
    fm_vec3_norm(&res, &res);
    return res;
  }

  inline fm_vec3_t randDir2D()
  {
    fm_vec3_t res{{rand01()-0.5f, 0.0f, rand01()-0.5f}};
    fm_vec3_norm(&res, &res);
    return res;
  }
}
