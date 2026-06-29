/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "vecMath.h"
#include "raycast.h"

namespace P64::Coll {

  struct SphereSweepHit {
    fm_vec3_t normal{};   ///< Contact normal pointing away from geometry, toward the capsule
    fm_vec3_t point{};    ///< Contact point on the geometry surface
    float t{1.0f};        ///< Normalized fraction [0..1] of displacement at first contact
    float depth{0.0f};    ///< Overlap depth when t == 0 (capsule was already penetrating)
    bool didHit{false};
  };

  /**
   * Tests a sphere sweep against a single world-space triangle.
   * Returns true and fills `hit` with the earliest contact (or deepest overlap when t == 0).
   *
   * @param center         Capsule center in world space
   * @param radius         Capsule radius
   * @param displacement   World-space displacement vector (not normalized)
   * @param v0/v1/v2       World-space triangle vertices
   * @param triNormal      World-space outward triangle normal (should be normalized)
   */
  bool sphereSweepTriangle(
    const fm_vec3_t& center,
    float radius,
    const fm_vec3_t& displacement,
    const fm_vec3_t& v0,
    const fm_vec3_t& v1,
    const fm_vec3_t& v2,
    const fm_vec3_t& triNormal,
    SphereSweepHit& hit
  );

}
