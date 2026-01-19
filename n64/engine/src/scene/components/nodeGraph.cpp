/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/nodeGraph.h"

#include "assets/assetManager.h"

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
  };
}

namespace P64::Comp
{
  void NodeGraph::initDelete(Object &obj, NodeGraph* data, uint16_t* initData_)
  {
    auto initData = (InitData*)initData_;
    if (initData == nullptr) {
      data->~NodeGraph();
      return;
    }

    new(data) NodeGraph();

    data->inst = P64::NodeGraph::Instance(initData->assetIdx);
  }
}
