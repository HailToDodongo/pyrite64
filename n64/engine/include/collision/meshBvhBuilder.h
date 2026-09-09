#pragma once

#include "meshBvh.h"
#include <algorithm>
#include <memory>
#include <numeric>

namespace P64::Coll {

  // Used by the Editor during build and by manually created runtime meshes only.
  // Median splits along the widest centroid axis guarantee logarithmic depth.
  // triangleBounds(index) returns a MeshBvhNode with its min/max initialized.
  template<typename TriangleBounds>
  std::unique_ptr<MeshBvhNode[]> buildMeshBvh(uint16_t triangleCount, TriangleBounds triangleBounds) {
    if(!triangleCount) return {};
    auto nodes = std::make_unique<MeshBvhNode[]>(meshBvhNodeCount(triangleCount));
    auto indices = std::make_unique<uint16_t[]>(triangleCount);
    std::iota(indices.get(), indices.get() + triangleCount, uint16_t{0});

    auto build = [&](auto &&self, uint32_t nodeIndex, uint32_t begin, uint32_t end) -> void {
      auto &node = nodes[nodeIndex];
      node = triangleBounds(indices[begin]);
      float centroidMin[3], centroidMax[3];
      for(int axis = 0; axis < 3; ++axis) {
        centroidMin[axis] = centroidMax[axis] = node.min[axis] * 0.5f + node.max[axis] * 0.5f;
      }
      for(uint32_t i = begin + 1; i < end; ++i) {
        const auto bounds = triangleBounds(indices[i]);
        for(int axis = 0; axis < 3; ++axis) {
          node.min[axis] = std::min(node.min[axis], bounds.min[axis]);
          node.max[axis] = std::max(node.max[axis], bounds.max[axis]);
          const float centroid = bounds.min[axis] * 0.5f + bounds.max[axis] * 0.5f;
          centroidMin[axis] = std::min(centroidMin[axis], centroid);
          centroidMax[axis] = std::max(centroidMax[axis], centroid);
        }
      }
      if(end - begin == 1) {
        node.escapeOrTriangle = MeshBvhNode::LEAF | indices[begin];
        return;
      }
      int axis = 0;
      for(int a = 1; a < 3; ++a) {
        if(centroidMax[a] - centroidMin[a] > centroidMax[axis] - centroidMin[axis]) axis = a;
      }
      const uint32_t middle = begin + (end - begin) / 2;
      std::nth_element(indices.get() + begin, indices.get() + middle, indices.get() + end,
        [&](uint16_t a, uint16_t b) {
          const auto boundsA = triangleBounds(a);
          const auto boundsB = triangleBounds(b);
          const float ca = boundsA.min[axis] * 0.5f + boundsA.max[axis] * 0.5f;
          const float cb = boundsB.min[axis] * 0.5f + boundsB.max[axis] * 0.5f;
          return ca != cb ? ca < cb : a < b;
        });
      node.escapeOrTriangle = (end - begin) * 2 - 1;
      self(self, nodeIndex + 1, begin, middle);
      self(self, nodeIndex + (middle - begin) * 2, middle, end);
    };
    build(build, 0, 0, triangleCount);
    return nodes;
  }

}
