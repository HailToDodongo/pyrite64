/**
* @copyright 2023 - Max Bebök
* @license MIT
*/
#pragma once
#include <functional>
#include <t3d/t3dmath.h>

#include "lib/math.h"

namespace Coll
{
  struct BCS;

  namespace TriType {
    constexpr uint8_t FLOOR = 1 << 0;
    constexpr uint8_t WALL  = 1 << 1;
    constexpr uint8_t CEIL  = 1 << 2;
    constexpr uint8_t BCS   = 1 << 3;
  }

  namespace BCSFlags {
    constexpr uint8_t BOUNCY    = 1 << 0;
    constexpr uint8_t FIXED_XYZ = 1 << 1;
    constexpr uint8_t SHAPE_BOX = 1 << 2;
  }

  enum class CollType : uint8_t {
    PLAYER = 0,
    BOSS_HEAD = 1,
    BOSS_BODY = 2,
    SWORD = 3,
    COIN = 4,
    LIFE = 5,
    DESTRUCTABLE = 6,
  };

  namespace Mask {
    constexpr uint8_t TRI_MESH    = 1 << 0;
    constexpr uint8_t PLAYER      = 1 << 1;
    constexpr uint8_t ATTACK      = 1 << 2;
    constexpr uint8_t BREAKABLE   = 1 << 3;
    constexpr uint8_t COLLECTABLE = 1 << 4;
    constexpr uint8_t SOLID       = 1 << 5;
    constexpr uint8_t HEAVY       = 1 << 6;
    constexpr uint8_t UNUSED7     = 1 << 7;
  }

  struct IVec3 {
    int16_t v[3]{};
  };

  struct AABB
  {
    IVec3 min{};
    IVec3 max{};

    bool vsAABB(const AABB &other) const;
    bool vs2DPointY(const IVec3 &pos) const;
  };
  static_assert(sizeof(AABB) == (6 * sizeof(int16_t)));

  struct BCS
  {
    fm_vec3_t center{};
    fm_vec3_t halfExtend{};
    fm_vec3_t velocity{};
    //std::function<void(BCS&)> callback{};
    uint8_t maskRead{0};
    uint8_t maskWrite{0};
    uint8_t flags{0};
    uint8_t hitTriTypes{0}; // mask of triangle types the sphere last collided with
    CollType type{};

    [[nodiscard]] constexpr float getRadius() const {
      return halfExtend.y;
    }
    [[nodiscard]] constexpr float getRadius2() const {
      return halfExtend.y * halfExtend.y;
    }

    [[nodiscard]] fm_vec3_t getMinAABB() const {
      return center - halfExtend;
    }
    [[nodiscard]] fm_vec3_t getMaxAABB() const {
      return center + halfExtend;
    }

    BCS operator*(float scale) const {
      return {
        .center = center * scale,
        .halfExtend = halfExtend * scale
      };
    }

    AABB toAABB() const {
      auto min = center - halfExtend;
      auto max = center + halfExtend;
      return {
        .min = {{ (int16_t)min.v[0], (int16_t)min.v[1], (int16_t)min.v[2] }},
        .max = {{ (int16_t)max.v[0], (int16_t)max.v[1], (int16_t)max.v[2] }}
      };
    }
  };

  struct CollInfo
  {
    fm_vec3_t penetration{};
    fm_vec3_t floorWallAngle{};
    int collCount{};
  };

  struct RaycastRes {
    fm_vec3_t hitPos{};
    fm_vec3_t normal{};

    [[nodiscard]] bool hasResult() const {
      return normal.y != 0.0f;
    }
  };

  struct Triangle
  {
    fm_vec3_t normal{};
    fm_vec3_t* v[3]{};
    AABB aabb{};
  };

  struct Triangle2D {
    fm_vec2_t v[3]{};
  };
}
