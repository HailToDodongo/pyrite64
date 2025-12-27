/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/componentTable.h"

#include "scene/components/code.h"
#include "scene/components/model.h"
#include "scene/components/light.h"
#include "scene/components/camera.h"
#include "scene/components/collMesh.h"
#include "scene/components/collBody.h"
#include "scene/components/audio2d.h"

#define SET_COMP(name) \
  [Comp::name::ID] = { \
    .initDel = reinterpret_cast<FuncInitDel>(Comp::name::initDelete), \
    .update = reinterpret_cast<FuncUpdate>(Comp::name::update), \
    .draw   = reinterpret_cast<FuncDraw>(Comp::name::draw), \
    .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::name::getAllocSize), \
  }

#define SET_COMP_NO_DRAW(name) \
  [Comp::name::ID] = { \
  .initDel = reinterpret_cast<FuncInitDel>(Comp::name::initDelete), \
  .update = reinterpret_cast<FuncUpdate>(Comp::name::update), \
  .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::name::getAllocSize), \
  }

#define SET_EVENT_COMP(name) \
  [Comp::name::ID] = { \
  .initDel = reinterpret_cast<FuncInitDel>(Comp::name::initDelete), \
  .update = reinterpret_cast<FuncUpdate>(Comp::name::update), \
  .draw   = reinterpret_cast<FuncDraw>(Comp::name::draw), \
  .onEvent = reinterpret_cast<FuncOnEvent>(Comp::name::onEvent), \
  .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::name::getAllocSize), \
  }

namespace P64
{
  const ComponentDef COMP_TABLE[COMP_TABLE_SIZE] {
    [Comp::Code::ID] = {
      .initDel = reinterpret_cast<FuncInitDel>(Comp::Code::initDelete),
      .update = reinterpret_cast<FuncUpdate>(Comp::Code::update),
      .draw   = reinterpret_cast<FuncDraw>(Comp::Code::draw),
      .onEvent = reinterpret_cast<FuncOnEvent>(Comp::Code::onEvent),
      .onColl = reinterpret_cast<FuncOnColl>(Comp::Code::onColl),
      .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::Code::getAllocSize),
    },
    SET_COMP(Model),
    SET_COMP(Light),
    SET_COMP(Camera),
    SET_COMP_NO_DRAW(CollMesh),
    SET_COMP_NO_DRAW(CollBody),
    SET_COMP_NO_DRAW(Audio2D),
  };
}