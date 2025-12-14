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
    // mirrors the .coll file format
    uint32_t triCount{};
    uint32_t vertCount{};
    float collScale{};
    fm_vec3_t *verts{};
    IVec3 *normals{};
    BVH* bvh{};
    // data follows here: indices, normals, verts, BVH
    int16_t indices[];

    [[nodiscard]] Coll::CollInfo vsSphere(const Coll::BCS &sphere, const Triangle& triangle) const;
    [[nodiscard]] Coll::CollInfo vsBox(const Coll::BCS &box, const Triangle& triangle) const;
    [[nodiscard]] Coll::RaycastRes vsFloorRay(const fm_vec3_t &pos, const Triangle& triangle) const;

    static Mesh* load(const std::string &path);
  };

  struct MeshInstance {
    Mesh *mesh{};
    fm_vec3_t pos{0.0f, 0.0f, 0.0f};
    fm_vec3_t scale{1.0f, 1.0f, 1.0f};
    T3DQuat rot{0.0f, 0.0f, 0.0f, 1.0f};
  };
}