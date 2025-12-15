/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "collision.h"

#include "bvh/v2/bvh.h"
#include "bvh/v2/vec.h"
#include "bvh/v2/ray.h"
#include "bvh/v2/node.h"
#include "bvh/v2/default_builder.h"

#include <vector>

using Scalar  = double;
using BVec3   = bvh::v2::Vec<Scalar, 3>;
using BBox    = bvh::v2::BBox<Scalar, 3>;
using Node    = bvh::v2::Node<Scalar, 3>;
using Bvh     = bvh::v2::Bvh<Node>;

namespace
{
  void writeBVHNode(std::vector<int16_t> &out, Node &node, int nodeIndex) {
    // 'bounds' layout is [min_x, max_x, min_y, max_y, min_z, max_z]
    // we need min/max as separate vectors
    int16_t offset = 1;

    glm::i16vec3 min{
      (int16_t)round(node.bounds[0]) - offset,
      (int16_t)round(node.bounds[2]) - offset,
      (int16_t)round(node.bounds[4]) - offset
    };
    glm::i16vec3 max{
      (int16_t)round(node.bounds[1]) + offset,
      (int16_t)round(node.bounds[3]) + offset,
      (int16_t)round(node.bounds[5]) + offset
    };

    for(int i=0; i<3; ++i) {
      int diff = max[i] - min[i];
      if(diff < 8) {
        min[i] -= 4;
        max[i] += 4;
      }
    }

    out.push_back(min.x);
    out.push_back(min.y);
    out.push_back(min.z);
    out.push_back(max.x);
    out.push_back(max.y);
    out.push_back(max.z);

    int dataCount = node.index.value & 0b1111;
    int dataOffset = node.index.value >> 4;

    if(dataCount == 0) {
      int indexDiff = dataOffset - nodeIndex;

      int16_t packedVal = (int16_t)(indexDiff << 4);
      if((packedVal >> 4) != indexDiff) {
        printf("Error: indexDiff %d (%d - %d) does not fit in 12 bits\n", indexDiff, dataOffset, nodeIndex);
        throw;
      }
      //assert((packedVal >> 4) == indexDiff);
      out.push_back(packedVal);
    } else {
      out.push_back(node.index.value);
    }
  }

  void writeBVH(std::vector<int16_t> &out, Bvh &bvh) {
    out.push_back(bvh.nodes.size());
    out.push_back(bvh.prim_ids.size());
    int nodeIndex = 0;
    for(auto& node : bvh.nodes) {
      writeBVHNode(out, node, nodeIndex++);
    }
    for(auto&& prim_id : bvh.prim_ids) {
      out.push_back(prim_id);
    }
  }
}

std::vector<int16_t> Project::Assets::Collision::createBVH(
  const std::vector<glm::i16vec3> &vertices,
  const std::vector<uint16_t> &indices
) {
  std::vector<BBox> aabbs;
  std::vector<BVec3> centers;

  for(int i=0; i<indices.size(); i+=3) {
    auto &v0 = vertices[indices[i]];
    auto &v1 = vertices[indices[i+1]];
    auto &v2 = vertices[indices[i+2]];

    BBox aabb{  BVec3(v0[0], v0[1], v0[2])};
    aabb.extend(BVec3(v1[0], v1[1], v1[2]));
    aabb.extend(BVec3(v2[0], v2[1], v2[2]));

    aabbs.push_back(aabb);
    centers.push_back(aabb.get_center());
  }

  bvh::v2::ThreadPool thread_pool;
  typename bvh::v2::DefaultBuilder<Node>::Config config;
  config.quality = bvh::v2::DefaultBuilder<Node>::Quality::High;
  auto bvh = bvh::v2::DefaultBuilder<Node>::build(thread_pool, aabbs, centers, config);

  std::vector<int16_t> treeData;
  writeBVH(treeData, bvh);
  return treeData;
}
