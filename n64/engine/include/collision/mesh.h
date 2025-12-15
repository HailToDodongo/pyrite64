/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>
#include <string>
#include "shapes.h"

namespace Coll
{
  struct BVH;

  struct Mesh
  {
    // NOTE: don't place any extra members here!
    // mirrors the collion data in the t3dm extension
    uint32_t triCount{};
    uint32_t vertCount{};
    float collScale{};
    fm_vec3_t *verts{};
    IVec3 *normals{};
    BVH* bvh{};
    // data follows here: indices, normals, verts, BVH
    int16_t indices[];

    [[nodiscard]] CollInfo vsSphere(const BCS &sphere, const Triangle& triangle) const;
    [[nodiscard]] CollInfo vsBox(const BCS &box, const Triangle& triangle) const;
    [[nodiscard]] RaycastRes vsFloorRay(const fm_vec3_t &pos, const Triangle& triangle) const;

    static Mesh* load(void* rawData);
  };

  struct MeshInstance {
    Mesh *mesh{};
    fm_vec3_t pos{0.0f, 0.0f, 0.0f};
    fm_vec3_t scale{1.0f, 1.0f, 1.0f};
    fm_quat_t rot{0.0f, 0.0f, 0.0f, 1.0f};

    fm_vec3_t intoLocalSpace(const fm_vec3_t &p) const {
      return (p / scale) - pos;
    }
    fm_vec3_t outOfLocalSpace(const fm_vec3_t &p) const {
      return (p + pos) * scale;
    }
  };
}