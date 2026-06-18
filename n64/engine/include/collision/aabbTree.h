/**
 * @file aabbTree.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Dynamic AABB tree BVH Implementation
 */
#pragma once

#include "vecMath.h"
#include "shapes.h"
#include <cstdint>
#include <cassert>
#include <memory>

namespace P64::Coll {

  using NodeProxy = int16_t;
  constexpr NodeProxy NULL_NODE = -1;
  // Multiplier for how much to fatten AABBs when inserting/moving nodes. This helps avoid frequent reinsertion for small movements.
  constexpr float AABB_DISPLACEMENT_MULTIPLIER = 10.0f;
  constexpr int AABB_QUERY_STACK_SIZE = 256;

  // Forward declare Raycast for ray queries
  struct Raycast;

  // ── Tree node ─────────────────────────────────────────────────────

  struct AABBTreeNode {
    AABB bounds{};
    NodeProxy parent{NULL_NODE};
    NodeProxy left{NULL_NODE};
    NodeProxy right{NULL_NODE};
    NodeProxy next{NULL_NODE};
    void *data{nullptr};
  };

  // ── AABB Tree (dynamic BVH) ───────────────────────────────────────

  class AABBTree {
  public:
    AABBTree() = default;
    ~AABBTree();

    void init(int capacity);
    void destroy();

    NodeProxy createNode(const AABB &bounds, void *data);
    bool moveNode(NodeProxy node, const AABB &aabb, const fm_vec3_t &displacement);
    void removeLeaf(NodeProxy leaf, bool freeIt);

    [[nodiscard]] void *getNodeData(NodeProxy node) const;
    [[nodiscard]] const AABB *getNodeBounds(NodeProxy node) const;
    [[nodiscard]] bool isLeaf(NodeProxy node) const;

    int queryBounds(const AABB &queryBox, NodeProxy *results, int maxResults) const;
    int queryPoint(const fm_vec3_t &point, NodeProxy *results, int maxResults) const;
    int queryRay(const Raycast &ray, NodeProxy *results, int maxResults) const;

    // Always order so that id_a < id_b
    static int32_t makeNodePairKey(NodeProxy id_a, NodeProxy id_b)
    {
      if (id_a > id_b)
        std::swap(id_a, id_b);
      return ((int32_t)id_a << 16) | id_b;
    }

    NodeProxy root{NULL_NODE};

  private:
    NodeProxy allocateNode();
    void freeNode(NodeProxy node);
    NodeProxy insertLeaf(NodeProxy leaf);
    void rotateNode(NodeProxy node);

    std::unique_ptr<AABBTreeNode[]> nodes_;
    int16_t nodeCount_{0};
    int16_t nodeCapacity_{0};
    NodeProxy freeList_{NULL_NODE};
  };

} // namespace P64::Coll
