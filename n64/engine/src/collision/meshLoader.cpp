/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "collision/mesh.h"
#include "collision/bvh.h"

namespace {
 char* align(char* ptr, size_t alignment) {
   return (char*)(((uintptr_t)ptr + alignment - 1) & ~(alignment - 1));
 }

   static void debugDrawBVTreeNode(
    const int16_t *data, uint32_t basePtr,
    const Coll::BVHNode *node, int level
  ) {
    int dataCount = node->value & 0b1111;
    int offset = (int16_t)node->value >> 4;
    // indent
    for(int i = 0; i < level; i++)debugf("  ");
    //debugf("%d %d %d - %d %d %d\n", node->aabbMin[0], node->aabbMin[1], node->aabbMin[2], node->aabbMax[0], node->aabbMax[1], node->aabbMax[2]);
    if(dataCount == 0) {
      debugDrawBVTreeNode(data, basePtr, &node[offset], level+1);
      debugDrawBVTreeNode(data, basePtr, &node[offset+1], level+1);
    } else {
      for(int i = 0; i < level; i++)debugf("  ");
      debugf("## Data: ");

      int offsetEnd = offset + dataCount;
      while(offset < offsetEnd) {
        debugf("%d ", data[offset++]);
      }
      debugf("\n");
    }
  }

  [[maybe_unused]] static void debugDrawBVTree(const Coll::BVH *bvh) {
    const int16_t *data = (int16_t*)&bvh->nodes[bvh->nodeCount]; // data starts right after nodes
    uint32_t basePtr = (uint32_t)(char*)bvh;
    debugDrawBVTreeNode(data, basePtr, bvh->nodes, 0);
  }
}

Coll::Mesh* Coll::Mesh::load(const std::string &path)
{
  int fileSize = 0;
  Mesh* mesh = (Mesh*)asset_load(path.c_str(), &fileSize);

  //debugf("Loading collision mesh %s, size: %d\n", path.c_str(), fileSize);

  char* data = (char*)&mesh->indices[0];

  data += mesh->triCount * sizeof(int16_t) * 3;
  data = align(data, 4);
  mesh->normals = (IVec3*)data;

  data += mesh->triCount * sizeof(IVec3);
  data = align(data, 4);
  mesh->verts = (T3DVec3 *)data;

  data += mesh->vertCount * sizeof(T3DVec3);
  data = align(data, 4);
  mesh->bvh = (BVH*)data;

  //debugf("BVH: %d nodes, %d data\n", mesh->bvh->nodeCount, mesh->bvh->dataCount);
  //debugDrawBVTree(mesh->bvh);

  return mesh;
}
