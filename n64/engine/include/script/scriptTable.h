/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include "nodeGraph.h"

namespace P64 { class Object; struct ObjectEvent; }
namespace P64::Coll { struct CollEvent; }

namespace P64::Script
{
  typedef void(*FuncObjInit)(Object&, void*);
  typedef void(*FuncObjDataDelta)(Object&, void*, float);
  typedef void(*FuncObjDataEvent)(Object&, void*, const ObjectEvent&);
  typedef void(*FuncObjDataColl)(Object&, void*, const P64::Coll::CollEvent&);

  struct ScriptEntry
  {
    FuncObjInit init;
    FuncObjInit destroy;
    FuncObjDataDelta update;
    FuncObjDataDelta fixedUpdate;
    FuncObjDataDelta draw;
    FuncObjDataEvent onEvent;
    FuncObjDataColl onColl;
    const void* const* dataType; // points to the 'DATA_TYPE' of the script, see 'TYPE_TAG'
  };

  // Note: generated and implement in the project:
  ScriptEntry &getCodeByIndex(uint32_t idx);
  uint16_t getCodeSizeByIndex(uint32_t idx);
  NodeGraph::GraphFunc getGraphFuncByUUID(uint64_t uuid);
}