/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include "collision/shapes.h"

namespace {
  constexpr float AABB_EPSILON = 0.001f;
}

bool Coll::AABB::vsAABB(const Coll::AABB &other) const {
  return (max.v[0] >= other.min.v[0])
      && (max.v[1] >= other.min.v[1])
      && (max.v[2] >= other.min.v[2])
      && (min.v[0] <= other.max.v[0])
      && (min.v[1] <= other.max.v[1])
      && (min.v[2] <= other.max.v[2]);
}

bool Coll::AABB::vs2DPointY(const IVec3 &pos) const {
  return (max.v[0] >= pos.v[0])
      && (max.v[2] >= pos.v[2])
      && (min.v[0] <= pos.v[0])
      && (min.v[2] <= pos.v[2]);
}
