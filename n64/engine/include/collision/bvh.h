/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>
#include "shapes.h"

namespace Coll
{
  constexpr int MAX_RESULT_COUNT = 32;

  struct BVHResult {
    int16_t triIndex[MAX_RESULT_COUNT]{};
    int16_t count{};

    void reset() { count = 0; }
  };

  struct BVHNode {
    AABB aabb{};
    uint16_t value{};
  };
  static_assert(sizeof(BVHNode) == (7 * sizeof(int16_t)));

  struct BVH {
    uint16_t nodeCount;
    uint16_t dataCount;
    BVHNode nodes[];
    // uint16_t data[];

    void vsAABB(const AABB &aabb, BVHResult &res) const;

    inline void vsBCS(const BCS &bcs, BVHResult &res) const {
      vsAABB((bcs * 64.0f).toAABB(), res);
    }

    void raycastFloor(const Coll::IVec3 &pos, BVHResult &res) const;
  };
}