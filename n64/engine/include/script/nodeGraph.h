/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

#include "assets/assetManager.h"

namespace P64::NodeGraph
{
  struct GraphDef;
  struct NodeDef;

  class Instance
  {
    private:
      GraphDef* graphDef{};

      NodeDef* currNode{};
      uint16_t reg{};

    public:
      Instance() = default;

      explicit Instance(uint16_t assetIdx) {
        graphDef = (GraphDef*)AssetManager::getByIndex(assetIdx);
        currNode = (NodeDef*)((char*)graphDef + 2);
      }

      void update(float deltaTime);
  };
}
