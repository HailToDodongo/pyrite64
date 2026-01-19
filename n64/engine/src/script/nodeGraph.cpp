/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "script/nodeGraph.h"

namespace P64::NodeGraph
{
  struct NodeDef
  {
    uint8_t type{};
    uint8_t outCount{};
    uint16_t outOffsets[];
  };

  struct GraphDef
  {
    uint16_t nodeCount;
    NodeDef start;
  };

  void printNode(NodeDef* node, int level)
  {
    debugf("%*s", level * 2, "");
    debugf("Type:%d, outputs: %d ", node->type, node->outCount);
    for (uint16_t i = 0; i < node->outCount; i++) {
      debugf("0x%04X ", node->outOffsets[i]);
    }

    uint16_t *nodeData = (uint16_t*)&node->outOffsets[node->outCount];
    //debugf(", data: %04X", *nodeData);
    debugf("\n");

    for (uint16_t i = 0; i < node->outCount; i++) {
      auto nextNode = (NodeDef*)((uint8_t*)node + node->outOffsets[i]);
      printNode(nextNode, level + 1);
    }
  };

}

void P64::NodeGraph::Instance::update(float deltaTime) {
  printNode(currNode, 0);
}
