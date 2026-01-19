/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/componentTable.h"
#include "scene/scene.h"
#include <type_traits>

#include "scene/components/code.h"
#include "scene/components/model.h"
#include "scene/components/light.h"
#include "scene/components/camera.h"
#include "scene/components/collMesh.h"
#include "scene/components/collBody.h"
#include "scene/components/audio2d.h"
#include "scene/components/constraint.h"
#include "scene/components/culling.h"
#include "scene/components/nodeGraph.h"

// some template magic to auto-detect if a function exists in a component
#define HAS_FUNC_TPL(NAME_HAS, NAME_GET, FUNC) \
  template<typename T, typename = void> \
  struct NAME_HAS : std::false_type {}; \
  \
  template<typename T> \
  struct NAME_HAS<T, std::void_t<decltype(&T::FUNC)>> : std::true_type {}; \
  \
  template<typename T> \
  auto NAME_GET() { \
    if constexpr (NAME_HAS<T>::value) { return &T::FUNC; } else { return nullptr; } \
  }

namespace
{
  HAS_FUNC_TPL(has_draw,   get_draw,    draw   )
  HAS_FUNC_TPL(has_update, get_update,  update )
  HAS_FUNC_TPL(has_event,  get_event,   onEvent)
  HAS_FUNC_TPL(has_coll,   get_coll,    onColl )
}

#define SET_COMP(NAME) \
  [Comp::NAME::ID] = { \
    .initDel = reinterpret_cast<FuncInitDel>(Comp::NAME::initDelete), \
    .update = (FuncUpdate)get_update<Comp::NAME>(), \
    .draw   = (FuncDraw)(get_draw<Comp::NAME>()), \
    .onEvent = (FuncOnEvent)(get_event<Comp::NAME>()), \
    .onColl = (FuncOnColl)(get_coll<Comp::NAME>()), \
    .getAllocSize = reinterpret_cast<FuncGetAllocSize>(Comp::NAME::getAllocSize), \
  }

namespace P64
{
  const ComponentDef COMP_TABLE[COMP_TABLE_SIZE] {
    SET_COMP(Code),
    SET_COMP(Model),
    SET_COMP(Light),
    SET_COMP(Camera),
    SET_COMP(CollMesh),
    SET_COMP(CollBody),
    SET_COMP(Audio2D),
    SET_COMP(Constraint),
    SET_COMP(Culling),
    SET_COMP(NodeGraph),
  };
}
