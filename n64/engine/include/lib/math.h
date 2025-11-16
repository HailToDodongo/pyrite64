/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

// global to mimic livdragons types
union fm_vec2_t {
  struct {
    float x, y;
  };
  float v[2];

  float &operator[](size_t idx) {
    return v[idx];
  }
  const float &operator[](size_t idx) const {
    return v[idx];
  }

  inline float dot(const fm_vec2_t &other) {
    return x * other.x + y * other.y;
  }
};

namespace P64::Math
{
  constexpr float SQRT_2_INV = 0.70710678118f;

  constexpr static float s10ToFloat(uint32_t value, float offset, float scale) {
    return (float)value / 1023.0f * scale + offset;
  }

  [[maybe_unused]]
  static fm_quat_t unpackQuat(uint32_t quatPacked)
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

  static float easeOutCubic(float x) {
    x = 1.0f - x;
    return 1.0f - (x*x*x);
  }

  static float easeInOutCubic(float x) {
    x *= 2.0f;
    if(x < 1.0f) {
      return 0.5f * x * x * x;
    }
    x -= 2.0f;
    return 0.5f * (x * x * x + 2.0f);
  }

  static float easeOutSin(float x) {
    return fm_sinf((x * T3D_PI) * 0.5f);
  }

  static int noise2d(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (n * (n * n * 60493 + 19990303) + 89);
  }

  static float rand01() {
    return (rand() % 4096) / 4096.0f;
  }

  static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
  }

  template<typename T>
  static auto min(T a, T b) { return a < b ? a : b; }

  template<typename T>
  static auto max(T a, T b) { return a > b ? a : b; }

  template<typename T>
  static auto clamp(T val, T min, T max) {
    return val < min ? min : (val > max ? max : val);
  }

  static auto min(const fm_vec3_t &a) {
    return fminf(a.x, fminf(a.y, a.z));
  }
  static auto max(const fm_vec3_t &a) {
    return fmaxf(a.x, fmaxf(a.y, a.z));
  }

  static auto min(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)}};
  }
  static auto max(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)}};
  }

  static auto abs(const fm_vec3_t &a) {
    return fm_vec3_t{{fabsf(a.x), fabsf(a.y), fabsf(a.z)}};
  }

  static auto cross(const fm_vec3_t &a, const fm_vec3_t &b) {
    fm_vec3_t res;
    t3d_vec3_cross(res, a, b);
    return res;
  }

  static fm_vec3_t sign(const fm_vec3_t &v) {
    return fm_vec3_t{{
      v.x < 0.0f ? -1.0f : (v.x > 0.0f ? 1.0f : 0.0f),
      v.y < 0.0f ? -1.0f : (v.y > 0.0f ? 1.0f : 0.0f),
      v.z < 0.0f ? -1.0f : (v.z > 0.0f ? 1.0f : 0.0f)
    }};
  }

  static fm_vec3_t randDir3D() {
    fm_vec3_t res{{rand01()-0.5f, rand01()-0.5f, rand01()-0.5f}};
    t3d_vec3_norm(&res);
    return res;
  }

  static fm_vec3_t randDir2D()
  {
    fm_vec3_t res{{rand01()-0.5f, 0.0f, rand01()-0.5f}};
    t3d_vec3_norm(&res);
    return res;
  }
}
