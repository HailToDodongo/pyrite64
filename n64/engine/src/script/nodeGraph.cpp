/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "script/nodeGraph.h"

#include <unordered_set>

#include "scene/object.h"
#include "scene/scene.h"
#include "script/scriptTable.h"

namespace
{
  std::unordered_map<uint32_t, P64::NodeGraph::UserFunc> userFunctionMap{};
}

namespace P64::NodeGraph
{
  struct GraphDef
  {
    GraphFunc func;
    uint32_t _padding;
    uint16_t stackSize;
    uint16_t _pad2;
    uint32_t varBytes; // size of the per-instance variable blob
  };

  void* load(const char* path)
  {
    auto data = asset_load(path, nullptr);
    uint64_t uuid = ((uint64_t*)data)[0];
    // debugf("Loaded NodeGraph: %s (UUID: %016llX)\n", path, uuid);
    ((GraphFunc*)data)[0] = P64::Script::getGraphFuncByUUID(uuid);
    return data;
  }
}

void P64::NodeGraph::Instance::load(uint16_t assetIdx)
{
  asset = assetIdx;
  graphDef = (GraphDef*)AssetManager::getByIndex(asset);
  corot = coro_create(graphDef->func, this, graphDef->stackSize*2);
}

P64::NodeGraph::Instance::~Instance()
{
  if(corot) {
    coro_destroy(corot);
    corot = nullptr;
  }
}

bool P64::NodeGraph::Instance::update([[maybe_unused]] float dt) {
  //debugf("Instance::update: %p\n", corot);
  if(!corot)return false;

  //auto t = get_ticks();
  //disable_interrupts();
  coro_resume(corot);
  //enable_interrupts();
  //t = get_ticks() - t;

  if(coro_finished(corot))
  {
    coro_destroy(corot);
    corot = nullptr;
    return false;
  }
  return true;
}

void P64::NodeGraph::registerFunction(uint32_t strCRC32, UserFunc fn)
{
  userFunctionMap[strCRC32] = fn;
}

P64::NodeGraph::UserFunc P64::NodeGraph::getFunction(uint64_t uuid)
{
  auto it = userFunctionMap.find((uint32_t)uuid);
  if(it != userFunctionMap.end()) {
    return it->second;
  }
  return nullptr;
}
