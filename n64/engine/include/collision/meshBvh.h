#pragma once

#include <cstdint>

namespace P64::Coll {

  // Collision asset layout, shared with the editor. Offsets are relative to the header;
  // all fields and BVH nodes are written big-endian. After the header come triangle
  // indices (3 x uint16_t), normals (3 x float), vertices (3 x float), then BVH nodes.
  // Each array after the indices starts at a 4-byte boundary. Runtime colliders borrow
  // these immutable arrays directly from the loaded asset.
  struct RawCollisionHeader {
    uint32_t triCount;
    uint32_t vertCount;
    float collScale;
    uint32_t bvhOffset;
  };
  static_assert(sizeof(RawCollisionHeader) == 16);

  // Preorder binary tree with exact float bounds. Internal nodes store the number
  // of nodes in their subtree, so a miss skips it without a traversal stack.
  // Leaves store the original triangle index; geometry never needs reordering.
  struct MeshBvhNode {
    float min[3];
    float max[3];
    uint32_t escapeOrTriangle;

    static constexpr uint32_t LEAF = 0x80000000u;
    bool isLeaf() const { return (escapeOrTriangle & LEAF) != 0; }
    uint16_t triangleIndex() const { return static_cast<uint16_t>(escapeOrTriangle); }
  };
  static_assert(sizeof(MeshBvhNode) == 28);

  constexpr uint32_t meshBvhNodeCount(uint16_t triangleCount) {
    return triangleCount ? uint32_t(triangleCount) * 2 - 1 : 0;
  }

  template<typename Intersects>
  int queryMeshBvh(const MeshBvhNode *nodes, uint32_t nodeCount,
    uint16_t *results, int maxResults, Intersects intersects)
  {
    if(!nodes || !results || maxResults <= 0) return 0;
    int count = 0;
    for(uint32_t i = 0; i < nodeCount && count < maxResults;) {
      const auto &node = nodes[i];
      if(!intersects(node)) {
        i += node.isLeaf() ? 1 : node.escapeOrTriangle;
      } else {
        if(node.isLeaf()) results[count++] = node.triangleIndex();
        ++i;
      }
    }
    return count;
  }

}
