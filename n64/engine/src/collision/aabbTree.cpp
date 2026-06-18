/**
 * @file aabb_tree.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Dynamic AABB tree BVH Implementation (see aabbTree.h)
 */
#include "collision/aabbTree.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace P64::Coll;

// ── Lifecycle ───────────────────────────────────────────────────────

AABBTree::~AABBTree() {
  destroy();
}

void AABBTree::init(int capacity) {
  assert(capacity > 0);
  assert(capacity <= std::numeric_limits<int16_t>::max());
  nodeCapacity_ = static_cast<int16_t>(capacity);
  nodeCount_ = 0;
  root = NULL_NODE;

  nodes_ = std::make_unique<AABBTreeNode[]>(nodeCapacity_);

  // Build the free list: each node's `next` points to the following index.
  for(int16_t i = 0; i < nodeCapacity_ - 1; ++i) {
    nodes_[i].next = static_cast<NodeProxy>(i + 1);
    nodes_[i].parent = NULL_NODE;
    nodes_[i].left = NULL_NODE;
    nodes_[i].right = NULL_NODE;
  }
  nodes_[nodeCapacity_ - 1].next = NULL_NODE;
  nodes_[nodeCapacity_ - 1].parent = NULL_NODE;
  nodes_[nodeCapacity_ - 1].left = NULL_NODE;
  nodes_[nodeCapacity_ - 1].right = NULL_NODE;
  freeList_ = 0;
}

void AABBTree::destroy() {
  nodes_.reset();
  nodeCount_ = 0;
  nodeCapacity_ = 0;
  root = NULL_NODE;
  freeList_ = NULL_NODE;
}

// ── Node allocation ─────────────────────────────────────────────────

NodeProxy AABBTree::allocateNode() {
  // Grow the pool when the free list is exhausted.
  if(freeList_ == NULL_NODE) {
    int16_t oldCap = nodeCapacity_;
    assert(oldCap > 0);
    assert(oldCap <= (std::numeric_limits<int16_t>::max() / 2));
    nodeCapacity_ = static_cast<int16_t>(oldCap * 2);

    auto newNodes = std::make_unique<AABBTreeNode[]>(nodeCapacity_);
    std::copy_n(nodes_.get(), oldCap, newNodes.get());
    nodes_ = std::move(newNodes);

    // New nodes are value-initialized; chain them into the free list.
    for(int16_t i = oldCap; i < nodeCapacity_ - 1; ++i) {
      nodes_[i].next = static_cast<NodeProxy>(i + 1);
      nodes_[i].parent = NULL_NODE;
      nodes_[i].left = NULL_NODE;
      nodes_[i].right = NULL_NODE;
    }
    nodes_[nodeCapacity_ - 1].next = NULL_NODE;
    nodes_[nodeCapacity_ - 1].parent = NULL_NODE;
    nodes_[nodeCapacity_ - 1].left = NULL_NODE;
    nodes_[nodeCapacity_ - 1].right = NULL_NODE;
    freeList_ = oldCap;
  }

  NodeProxy id = freeList_;
  freeList_ = nodes_[id].next;

  nodes_[id].parent = NULL_NODE;
  nodes_[id].left = NULL_NODE;
  nodes_[id].right = NULL_NODE;
  nodes_[id].next = NULL_NODE;
  nodes_[id].data = nullptr;
  nodes_[id].bounds = {};
  ++nodeCount_;
  return id;
}

void AABBTree::freeNode(NodeProxy node) {
  assert(node >= 0 && node < nodeCapacity_);
  nodes_[node].parent = NULL_NODE;
  nodes_[node].left = NULL_NODE;
  nodes_[node].right = NULL_NODE;
  nodes_[node].data = nullptr;
  nodes_[node].next = freeList_;
  freeList_ = node;
  --nodeCount_;
}

// ── Public API ──────────────────────────────────────────────────────

NodeProxy AABBTree::createNode(const AABB &bounds, void *data) {
  NodeProxy id = allocateNode();
  // Fatten the AABB by some margin so small moves don't cause reinsertion.
  fm_vec3_t extent = (bounds.max - bounds.min) * 0.1f;
  nodes_[id].bounds.min = bounds.min - extent;
  nodes_[id].bounds.max = bounds.max + extent;
  nodes_[id].data = data;
  insertLeaf(id);
  return id;
}

bool AABBTree::moveNode(NodeProxy node, const AABB &aabb, const fm_vec3_t &displacement) {
  assert(node >= 0 && node < nodeCapacity_);
  assert(isLeaf(node));

  // If the fattened bounds still contain the tight AABB, skip reinsertion.
  if(aabbContains(nodes_[node].bounds, aabb)) {
    return false;
  }

  removeLeaf(node, false);

  // Recompute fattened bounds.
  fm_vec3_t extent = (aabb.max - aabb.min) * 0.1f;
  nodes_[node].bounds.min = aabb.min - extent;
  nodes_[node].bounds.max = aabb.max + extent;

  // Extend in the direction of movement.
  fm_vec3_t disp = displacement * AABB_DISPLACEMENT_MULTIPLIER;
  aabbExtendDirection(nodes_[node].bounds, disp, nodes_[node].bounds);

  insertLeaf(node);
  return true;
}

void AABBTree::removeLeaf(NodeProxy leaf, bool freeIt) {
  assert(leaf >= 0 && leaf < nodeCapacity_);

  if(leaf == root) {
    root = NULL_NODE;
    if(freeIt) freeNode(leaf);
    return;
  }

  NodeProxy parent = nodes_[leaf].parent;
  NodeProxy grandParent = nodes_[parent].parent;

  // The sibling is whichever child of parent is *not* the leaf.
  NodeProxy sibling = (nodes_[parent].left == leaf) ? nodes_[parent].right : nodes_[parent].left;

  if(grandParent != NULL_NODE) {
    // Replace the parent with the sibling in the grandparent's children.
    if(nodes_[grandParent].left == parent) {
      nodes_[grandParent].left = sibling;
    } else {
      nodes_[grandParent].right = sibling;
    }
    nodes_[sibling].parent = grandParent;
    freeNode(parent);

    // Walk up and refit ancestor bounds.
    NodeProxy idx = grandParent;
    while(idx != NULL_NODE) {
      NodeProxy l = nodes_[idx].left;
      NodeProxy r = nodes_[idx].right;
      if(l != NULL_NODE && r != NULL_NODE) {
        nodes_[idx].bounds = aabbUnion(nodes_[l].bounds, nodes_[r].bounds);
      }
      idx = nodes_[idx].parent;
    }
  } else {
    // Parent was the root — sibling becomes the new root.
    root = sibling;
    nodes_[sibling].parent = NULL_NODE;
    freeNode(parent);
  }

  nodes_[leaf].parent = NULL_NODE;
  if(freeIt) freeNode(leaf);
}

void *AABBTree::getNodeData(NodeProxy node) const {
  assert(node >= 0 && node < nodeCapacity_);
  return nodes_[node].data;
}

const AABB *AABBTree::getNodeBounds(NodeProxy node) const {
  assert(node >= 0 && node < nodeCapacity_);
  return &nodes_[node].bounds;
}

bool AABBTree::isLeaf(NodeProxy node) const {
  assert(node >= 0 && node < nodeCapacity_);
  return nodes_[node].left == NULL_NODE && nodes_[node].right == NULL_NODE;
}

// ── Leaf insertion (SAH) ────────────────────────────────────────────

NodeProxy AABBTree::insertLeaf(NodeProxy leaf) {
  if(root == NULL_NODE) {
    root = leaf;
    nodes_[leaf].parent = NULL_NODE;
    return leaf;
  }

  // Stage 1 — find the best sibling using a branch-and-bound SAH walk.
  AABB leafBounds = nodes_[leaf].bounds;
  NodeProxy bestSibling = root;
  float bestCost = aabbArea(aabbUnion(nodes_[root].bounds, leafBounds));

  // Candidate stack for the branch-and-bound search.
  struct Candidate { NodeProxy index; float inheritedCost; };
  Candidate stack[AABB_QUERY_STACK_SIZE];
  int stackCount = 0;

  auto pushCandidate = [&](NodeProxy idx, float inherited) {
    if(idx != NULL_NODE && stackCount < AABB_QUERY_STACK_SIZE) {
      stack[stackCount++] = {idx, inherited};
    }
  };

  pushCandidate(root, 0.0f);

  while(stackCount > 0) {
    Candidate c = stack[--stackCount];
    NodeProxy candidate = c.index;
    float inherited = c.inheritedCost;

    AABB combined = aabbUnion(nodes_[candidate].bounds, leafBounds);
    float directCost = aabbArea(combined);
    float cost = directCost + inherited;

    if(cost < bestCost) {
      bestCost = cost;
      bestSibling = candidate;
    }

    // Lower bound on cost for children of this candidate.
    float childInherited = inherited + directCost - aabbArea(nodes_[candidate].bounds);
    float lowerBound = aabbArea(leafBounds) + childInherited;
    if(lowerBound < bestCost && !isLeaf(candidate)) {
      pushCandidate(nodes_[candidate].left, childInherited);
      pushCandidate(nodes_[candidate].right, childInherited);
    }
  }

  // Stage 2 — create a new internal node and re-parent.
  NodeProxy oldParent = nodes_[bestSibling].parent;
  NodeProxy newParent = allocateNode();
  nodes_[newParent].parent = oldParent;
  nodes_[newParent].bounds = aabbUnion(leafBounds, nodes_[bestSibling].bounds);

  if(oldParent != NULL_NODE) {
    if(nodes_[oldParent].left == bestSibling) {
      nodes_[oldParent].left = newParent;
    } else {
      nodes_[oldParent].right = newParent;
    }
  } else {
    root = newParent;
  }

  nodes_[newParent].left = bestSibling;
  nodes_[newParent].right = leaf;
  nodes_[bestSibling].parent = newParent;
  nodes_[leaf].parent = newParent;

  // Stage 3 — walk back up and refit / rotate.
  NodeProxy idx = nodes_[leaf].parent;
  while(idx != NULL_NODE) {
    rotateNode(idx);
    NodeProxy l = nodes_[idx].left;
    NodeProxy r = nodes_[idx].right;
    nodes_[idx].bounds = aabbUnion(nodes_[l].bounds, nodes_[r].bounds);
    idx = nodes_[idx].parent;
  }

  return leaf;
}

// ── Tree rotation (balance heuristic) ───────────────────────────────

void AABBTree::rotateNode(NodeProxy node) {
  if(isLeaf(node)) return;

  NodeProxy left = nodes_[node].left;
  NodeProxy right = nodes_[node].right;

  // We evaluate 4 rotation candidates:
  // Swap left with right-left, left with right-right,
  // right with left-left, right with left-right.
  struct RotCandidate {
    NodeProxy parent;    // the child whose subtree changes
    NodeProxy child;     // the grandchild we swap in
    NodeProxy other;     // the other grandchild that stays
    NodeProxy swapWith;  // the node being swapped out (the sibling)
  };

  RotCandidate candidates[4];
  float costs[4];
  int numCandidates = 0;

  auto tryRotation = [&](NodeProxy parent, NodeProxy swapWith, NodeProxy child, NodeProxy other) {
    if(child == NULL_NODE) return;
    // Cost of the new parent after the swap = union of (swapWith, other).
    AABB newBounds = aabbUnion(nodes_[swapWith].bounds, nodes_[other].bounds);
    float costReduction = aabbArea(nodes_[parent].bounds) - aabbArea(newBounds);
    candidates[numCandidates] = {parent, child, other, swapWith};
    costs[numCandidates] = costReduction;
    ++numCandidates;
  };

  if(!isLeaf(right)) {
    tryRotation(right, left, nodes_[right].left, nodes_[right].right);
    tryRotation(right, left, nodes_[right].right, nodes_[right].left);
  }
  if(!isLeaf(left)) {
    tryRotation(left, right, nodes_[left].left, nodes_[left].right);
    tryRotation(left, right, nodes_[left].right, nodes_[left].left);
  }

  // Pick the rotation with the greatest cost reduction.
  int bestIdx = -1;
  float bestReduction = 0.0f;
  for(int i = 0; i < numCandidates; ++i) {
    if(costs[i] > bestReduction) {
      bestReduction = costs[i];
      bestIdx = i;
    }
  }

  if(bestIdx < 0) return;

  RotCandidate &rot = candidates[bestIdx];

  // Perform the swap: pull `child` out of `parent` and replace `swapWith`
  // under `node`, then push `swapWith` into `parent` where `child` was.
  if(nodes_[node].left == rot.swapWith) {
    nodes_[node].left = rot.child;
  } else {
    nodes_[node].right = rot.child;
  }
  nodes_[rot.child].parent = node;

  if(nodes_[rot.parent].left == rot.child) {
    nodes_[rot.parent].left = rot.swapWith;
  } else {
    nodes_[rot.parent].right = rot.swapWith;
  }
  nodes_[rot.swapWith].parent = rot.parent;

  // Refit the changed subtree parent.
  nodes_[rot.parent].bounds = aabbUnion(
    nodes_[nodes_[rot.parent].left].bounds,
    nodes_[nodes_[rot.parent].right].bounds
  );
}

// ── Queries ─────────────────────────────────────────────────────────

int AABBTree::queryBounds(const AABB &queryBox, NodeProxy *results, int maxResults) const {
  if(root == NULL_NODE) return 0;

  NodeProxy stack[AABB_QUERY_STACK_SIZE];
  int stackCount = 0;
  int resultCount = 0;

  stack[stackCount++] = root;

  while(stackCount > 0 && resultCount < maxResults) {
    NodeProxy current = stack[--stackCount];
    if(current == NULL_NODE) continue;

    if(!aabbOverlap(nodes_[current].bounds, queryBox)) continue;

    if(isLeaf(current)) {
      results[resultCount++] = current;
    } else {
      if(stackCount < AABB_QUERY_STACK_SIZE - 1) {
        stack[stackCount++] = nodes_[current].left;
        stack[stackCount++] = nodes_[current].right;
      }
    }
  }
  return resultCount;
}

int AABBTree::queryPoint(const fm_vec3_t &point, NodeProxy *results, int maxResults) const {
  if(root == NULL_NODE) return 0;

  NodeProxy stack[AABB_QUERY_STACK_SIZE];
  int stackCount = 0;
  int resultCount = 0;

  stack[stackCount++] = root;

  while(stackCount > 0 && resultCount < maxResults) {
    NodeProxy current = stack[--stackCount];
    if(current == NULL_NODE) continue;

    if(!aabbContainsPoint(nodes_[current].bounds, point)) continue;

    if(isLeaf(current)) {
      results[resultCount++] = current;
    } else {
      if(stackCount < AABB_QUERY_STACK_SIZE - 1) {
        stack[stackCount++] = nodes_[current].left;
        stack[stackCount++] = nodes_[current].right;
      }
    }
  }
  return resultCount;
}

int AABBTree::queryRay(const Raycast &ray, NodeProxy *results, int maxResults) const
{
  if(root == NULL_NODE) return 0;

  NodeProxy stack[AABB_QUERY_STACK_SIZE];
  int stackCount = 0;
  int resultCount = 0;

  stack[stackCount++] = root;

  while(stackCount > 0 && resultCount < maxResults) {
    NodeProxy current = stack[--stackCount];
    if(current == NULL_NODE) continue;

    if(!aabbIntersectsRay(nodes_[current].bounds, ray)) continue;

    if(isLeaf(current)) {
      results[resultCount++] = current;
    } else {
      if(stackCount < AABB_QUERY_STACK_SIZE - 1) {
        stack[stackCount++] = nodes_[current].left;
        stack[stackCount++] = nodes_[current].right;
      }
    }
  }
  return resultCount;
}
