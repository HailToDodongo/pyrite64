/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "collision/bvh.h"
// #include "../debug/debugDraw.h"

namespace {
  const int16_t *ctxData;
  const Coll::AABB *ctxAABB;
  const Coll::IVec3 *ctxRayPos;
  Coll::BVHResult *ctxRes;

  void queryNodeAABB(const Coll::BVHNode *node)
  {
    if(!node->aabb.vsAABB(*ctxAABB))return;

    int dataCount = node->value & 0b1111;
    int offset = (int16_t)node->value >> 4;

    if(dataCount == 0) {
      queryNodeAABB(&node[offset]);
      queryNodeAABB(&node[offset + 1]);
      return;
    }

    int offsetEnd = offset + dataCount;
    while(offset < offsetEnd && ctxRes->count < Coll::MAX_RESULT_COUNT) {
      ctxRes->triIndex[ctxRes->count++] = ctxData[offset++];
    }
  }

  void queryNodeRaycastFloor(const Coll::BVHNode *node)
  {
    if(!node->aabb.vs2DPointY(*ctxRayPos))return;

    int dataCount = node->value & 0b1111;
    int offset = (int16_t)node->value >> 4;

    if(dataCount == 0) {
      queryNodeRaycastFloor(&node[offset]);
      queryNodeRaycastFloor(&node[offset + 1]);
      return;
    }

    int offsetEnd = offset + dataCount;
    while(offset < offsetEnd && ctxRes->count < Coll::MAX_RESULT_COUNT) {
      ctxRes->triIndex[ctxRes->count++] = ctxData[offset++];
    }
  }
}

void Coll::BVH::vsAABB(const Coll::AABB &aabb, BVHResult &res) const {
  ctxData = (int16_t*)&nodes[nodeCount]; // data starts right after nodes;
  ctxAABB = &aabb;
  ctxRes = &res;
  queryNodeAABB(nodes);
}

void Coll::BVH::raycastFloor(const Coll::IVec3 &pos, Coll::BVHResult &res) const {
  ctxData = (int16_t*)&nodes[nodeCount]; // data starts right after nodes;
  ctxRayPos = &pos;
  ctxRes = &res;
  queryNodeRaycastFloor(nodes);
}


