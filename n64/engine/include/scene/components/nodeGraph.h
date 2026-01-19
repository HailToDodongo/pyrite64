/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"
#include "script/nodeGraph.h"

namespace P64::Comp
{
  struct NodeGraph
  {
    static constexpr uint32_t ID = 9;

    P64::NodeGraph::Instance inst{};

    static uint32_t getAllocSize([[maybe_unused]] void* initData)
    {
      return sizeof(NodeGraph);
    }

    static void initDelete([[maybe_unused]] Object& obj, NodeGraph* data, uint16_t* initData);

    static void update(Object& obj, NodeGraph* data, float deltaTime) {
      data->inst.update(deltaTime);
    }
  };
}