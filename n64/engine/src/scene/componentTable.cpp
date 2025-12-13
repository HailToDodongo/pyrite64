/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/componentTable.h"

#include "scene/components/code.h"
#include "scene/components/model.h"
#include "scene/components/light.h"
#include "scene/components/camera.h"

#define SET_COMP(name) \
  { \
    .initDel = reinterpret_cast<FuncInitDel>(Comp::name::initDelete), \
    .update = reinterpret_cast<FuncUpdate>(Comp::name::update), \
    .draw   = reinterpret_cast<FuncDraw>(Comp::name::draw), \
    .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::name::getAllocSize), \
  }

#define SET_EVENT_COMP(name) \
  { \
  .initDel = reinterpret_cast<FuncInitDel>(Comp::name::initDelete), \
  .update = reinterpret_cast<FuncUpdate>(Comp::name::update), \
  .draw   = reinterpret_cast<FuncDraw>(Comp::name::draw), \
  .onEvent = reinterpret_cast<FuncOnEvent>(Comp::name::onEvent), \
  .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::name::getAllocSize), \
  }

namespace P64
{
  const ComponentDef COMP_TABLE[COMP_TABLE_SIZE] {
    SET_EVENT_COMP(Code),
    SET_COMP(Model),
    SET_COMP(Light),
    SET_COMP(Camera),
    {}
  };
}