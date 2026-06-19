/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/nodeGraph.h"

#include "assets/assetManager.h"
#include <string.h>

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
    data->inst.load(initData->assetIdx);
    data->inst.object = &obj;
    data->doUpdate = initData->autoRun != 0;

    // Variables live inline right after the component (reserved by getAllocSize)
    data->inst.vars = data->getVarData();

    uint8_t count = initData->objRefCount;
    if(count > P64::NodeGraph::MAX_OBJ_REFS)count = P64::NodeGraph::MAX_OBJ_REFS;
    for(uint8_t i=0; i<count; ++i) {
      data->inst.objRefs[i] = initData->objRefs[i];
    }

    const uint8_t* p = (const uint8_t*)(initData->objRefs + initData->objRefCount);
    uint32_t varBytes = 0;
    memcpy(&varBytes, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    if(varBytes > 0) {
      memcpy(data->inst.vars, p, varBytes);
    }
  }
}
