/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"
#include "script/nodeGraph.h"

#include <string.h>

namespace P64::Comp
{
  struct NodeGraph
  {
    static constexpr uint32_t ID = 9;

    // Serialized component init data
    struct InitData
    {
      uint16_t assetIdx;
      uint8_t autoRun;
      uint8_t _pad0; // was 'repeatable' (now a graph-level option)
      uint8_t objRefCount;
      uint8_t _pad;
      uint16_t objRefs[]; // objRefCount entries, resolved runtime object ids
      // Followed by: uint32_t varBytes; then varBytes bytes of variable defaults.
    };

    // Size of the graph's variable blob, stored right after the object refs.
    static uint32_t varBytesOf(const InitData* d)
    {
      const uint8_t* p = (const uint8_t*)(d->objRefs + d->objRefCount);
      uint32_t varBytes = 0;
      memcpy(&varBytes, p, sizeof(uint32_t));
      return varBytes;
    }

    private:
      P64::NodeGraph::Instance inst{};
      uint8_t doUpdate{};

    public:
      inline void* getVarData() { return (char*)this + sizeof(NodeGraph); }

      bool run(uint32_t arg0 = 0, uint32_t arg1 = 0)
      {
        inst.args[0] = arg0;
        inst.args[1] = arg1;
        auto oldState = !doUpdate;
        doUpdate = true;
        return oldState == false;
      }

      [[nodiscard]] bool isRunning() const { return doUpdate != 0; }

      void enable() { doUpdate = true; }
      void disable() { doUpdate = false; }

    static uint32_t getAllocSize(void* initData)
    {
      return sizeof(NodeGraph) + varBytesOf((const InitData*)initData);
    }

    static void initDelete([[maybe_unused]] Object& obj, NodeGraph* data, uint16_t* initData);

    static void update(Object& obj, NodeGraph* data, float deltaTime) {
      if(data->doUpdate) {
        if(!data->inst.update(deltaTime)) {
          data->doUpdate = false;
        }
      }
    }
  };
}
