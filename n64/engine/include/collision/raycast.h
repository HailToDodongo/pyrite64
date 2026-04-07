/**
 * @file raycast.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Raycast definitions and functions
 */
#pragma once

#include "vec_math.h"
#include <cstdint>
#include <cmath>
#include <limits>

namespace P64::Coll {

  struct Collider;

  constexpr int RAYCAST_MAX_COLLIDER_TESTS = 50;
  constexpr int RAYCAST_MAX_TRIANGLE_TESTS = 30;

  enum class RaycastColliderTypeFlags : uint8_t {
    MESH_COLLIDERS = (1 << 0),
    COLLIDER_BODIES  = (1 << 1),
    ALL             = 0xFF
  };

  inline RaycastColliderTypeFlags operator|(RaycastColliderTypeFlags a, RaycastColliderTypeFlags b) {
    return static_cast<RaycastColliderTypeFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
  }
  inline bool hasFlag(RaycastColliderTypeFlags m, RaycastColliderTypeFlags f) {
    return (static_cast<uint8_t>(m) & static_cast<uint8_t>(f)) != 0;
  }

  

  struct Raycast {
    fm_vec3_t origin{};
    fm_vec3_t dir{};
    fm_vec3_t invDir{};
    float maxDistance{};
    RaycastColliderTypeFlags collTypes{RaycastColliderTypeFlags::ALL};
    uint8_t readMask{0xFF};
    bool interactTrigger{false};

    static Raycast create(const fm_vec3_t &origin, const fm_vec3_t &dir, float maxDist,
                          RaycastColliderTypeFlags collTypes = RaycastColliderTypeFlags::ALL, bool interactTrigger = false,
                          uint8_t readMask = 0xFF);
  };

    /**
   * Represents the result of a raycast intersection test.
   * If didHit is true, point, normal, distance, and hitObjectId will contain valid data about the intersection.
   * If didHit is false, the ray did not intersect
   */
  struct RaycastHit {
    fm_vec3_t point{};
    fm_vec3_t normal{};
    float distance{std::numeric_limits<float>::max()};
    uint16_t hitObjectId{0};
    bool didHit{false};
  };

  bool ray_collider_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit);

  bool ray_triangle_intersection(const Raycast &ray, const fm_vec3_t &v0, const fm_vec3_t &v1, const fm_vec3_t &v2, const fm_vec3_t &tri_norm, RaycastHit &hit);

} // namespace P64::Coll
