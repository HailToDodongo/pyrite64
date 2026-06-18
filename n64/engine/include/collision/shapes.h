/**
 * @file shapes.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Properties and helper functions of the different basic Collider shapes
 */
#pragma once

#include "vecMath.h"
#include <cmath>
#include "aabb.h"

namespace P64::Coll {

  // ── Sphere ──────────────────────────────────────────────────────────

  /// @brief Defines the Sphere Collider Type
  struct SphereShape {
    float radius; // Radius of the Sphere

    fm_vec3_t support(const fm_vec3_t &dir) const {
      fm_vec3_t unit;
      fm_vec3_norm(&unit, &dir);
      if(vec3IsZero(unit)) unit = VEC3_UP;
      return fm_vec3_t{{unit.x * radius, unit.y * radius, unit.z * radius}};
    }

    AABB boundingBox(const fm_quat_t * /*rotation*/) const {
      return {fm_vec3_t{{-radius, -radius, -radius}}, fm_vec3_t{{radius, radius, radius}}};
    }

    fm_vec3_t inertiaTensor(float mass) const {
      float inertia = 0.4f * mass * radius * radius;
      return fm_vec3_t{{inertia, inertia, inertia}};
    }
  };

  // ── Box ─────────────────────────────────────────────────────────────

  /// @brief Defines the Box (OBB) Collider Type
  struct BoxShape {
    fm_vec3_t halfSize; // Vector describing half the size in every axis

    fm_vec3_t support(const fm_vec3_t &dir) const {
      return fm_vec3_t{{
        copysignf(halfSize.x, dir.x),
        copysignf(halfSize.y, dir.y),
        copysignf(halfSize.z, dir.z)
      }};
    }

    AABB boundingBox(const fm_quat_t *q) const;
    fm_vec3_t inertiaTensor(float mass) const;
  };

  // ── Capsule ─────────────────────────────────────────────────────────

  /// @brief Defines the Capsule Collider Type
  struct CapsuleShape {
    float radius; // The radius of the capsule
    float innerHalfHeight; // Half the height of the cylindrical part of the capsule (without the hemisphere ends)

    fm_vec3_t support(const fm_vec3_t &dir) const {
      fm_vec3_t unit;
      fm_vec3_norm(&unit, &dir);
      if(vec3IsZero(unit)) unit = VEC3_UP;

      float y = copysignf(innerHalfHeight, unit.y);
      return fm_vec3_t{{unit.x * radius, unit.y * radius + y, unit.z * radius}};
    }

    AABB boundingBox(const fm_quat_t *q) const;
    fm_vec3_t inertiaTensor(float mass) const;
  };

  // ── Cylinder ────────────────────────────────────────────────────────

  /// @brief Defines the Cylinder Collider Type
  struct CylinderShape {
    float radius; // The radius of the cylinder
    float halfHeight; // Half of the height of the cylinder

    fm_vec3_t support(const fm_vec3_t &dir) const;
    AABB boundingBox(const fm_quat_t *q) const;
    fm_vec3_t inertiaTensor(float mass) const;
  };

  // ── Cone ────────────────────────────────────────────────────────────

  /// @brief Defines the Cone Collider Type

  struct ConeShape {
    float radius; // The radius of the base of the cone
    float halfHeight; // Half of the height of the cone from base to top

    fm_vec3_t support(const fm_vec3_t &dir) const;
    AABB boundingBox(const fm_quat_t *q) const;
    fm_vec3_t inertiaTensor(float mass) const;
  };

  // ── Pyramid ─────────────────────────────────────────────────────────

  /// @brief Defines the Pyramid Collider Type
  ///
  /// Hint: The Base of a Pyramid Collider does not need to be square
  struct PyramidShape {
    float baseHalfWidthX;
    float baseHalfWidthZ;
    float halfHeight;

    fm_vec3_t support(const fm_vec3_t &dir) const;
    AABB boundingBox(const fm_quat_t *q) const;
    fm_vec3_t inertiaTensor(float mass) const;
  };

} // namespace P64::Coll
