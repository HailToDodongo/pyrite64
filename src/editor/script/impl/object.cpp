/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "impl.h"
#include "../../../context.h"
#include "../../../project/scene/object.h"
#include "../../../project/scene/scene.h"

#include <utility>

using namespace Editor::ScriptHost::Impl;
using JVec3 = Editor::ScriptHost::Impl::Vec3;
using JQuat = Editor::ScriptHost::Impl::Quat;

namespace
{
  JSClassID objClassId{};

  // always resolve fresh via UUID, otherwise we risk stale pointer shenanigans
  Project::Object* findObject(JSContext *c, uint32_t uuid)
  {
    auto *scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
    auto obj = scene ? scene->getObjectByUUID(uuid) : nullptr;
    if(!obj)JS_ThrowReferenceError(c, "object no longer exists");
    return obj.get();
  }

  JSValue newObjectHandle(JSContext *c, uint32_t uuid)
  {
    JSValue res = JS_NewObjectClass(c, objClassId);
    JS_SetOpaque(res, (void*)(uintptr_t)uuid);
    return res;
  }

  Project::Object* getObject(JSContext *c, JSValueConst self) {
    return findObject(c, (uint32_t)(uintptr_t)JS_GetOpaque(self, objClassId));
  }

  JSValue getUUID(JSContext *c, JSValueConst self)
  {
    auto *obj = getObject(c, self);
    return obj ? JS_NewUint32(c, obj->uuid) : JS_EXCEPTION;
  }

  JSValue getName(JSContext *c, JSValueConst self)
  {
    auto *obj = getObject(c, self);
    return obj ? JS_NewString(c, obj->name.c_str()) : JS_EXCEPTION;
  }

  JSValue setName(JSContext *c, JSValueConst self, JSValueConst val)
  {
    auto *obj = getObject(c, self);
    if(!obj)return JS_EXCEPTION;
    const char* str = JS_ToCString(c, val);
    if(!str)return JS_EXCEPTION;
    obj->name = str;
    JS_FreeCString(c, str);
    return JS_UNDEFINED;
  }

  template<typename T> struct JSType;
  template<> struct JSType<glm::vec3> { using type = JVec3; };
  template<> struct JSType<glm::quat> { using type = JQuat; };

  template<auto PROP>
  using PropType = decltype((std::declval<Project::Object&>().*PROP).value);

  // prefab instances get an override first, same as when editing in the inspector
  template<auto PROP>
  PropType<PROP>& resolveProp(Project::Object &obj, bool forWrite)
  {
    auto &prop = obj.*PROP;
    if(forWrite && obj.isPrefabInstance() && !ctx.isPrefabEditing(obj.uuid) && !obj.hasPropOverride(prop)) {
      obj.addPropOverride(prop);
    }
    return prop.resolve(obj.propOverrides);
  }

  template<auto PROP>
  JSValue getProp(JSContext *c, JSValueConst self)
  {
    using JS = typename JSType<PropType<PROP>>::type;
    auto *obj = getObject(c, self);
    if(!obj)return JS_EXCEPTION;
    return JS::create(c, [uuid = obj->uuid](JSContext *c, bool forWrite) -> PropType<PROP>* {
      auto *obj = findObject(c, uuid);
      return obj ? &resolveProp<PROP>(*obj, forWrite) : nullptr;
    });
  }

  template<auto PROP>
  JSValue setProp(JSContext *c, JSValueConst self, JSValueConst val)
  {
    using JS = typename JSType<PropType<PROP>>::type;
    auto *obj = getObject(c, self);
    if(!obj)return JS_EXCEPTION;
    PropType<PROP> v{};
    if(!JS::read(c, val, v))return JS_ThrowTypeError(c, "value has the wrong format");
    resolveProp<PROP>(*obj, true) = v;
    return JS_UNDEFINED;
  }

  JSValue objToJSON(JSContext *c, JSValueConst self, int, JSValueConst*)
  {
    auto *obj = getObject(c, self);
    if(!obj)return JS_EXCEPTION;
    JSValue res = JS_NewObject(c);
    JS_SetPropertyStr(c, res, "uuid", JS_NewUint32(c, obj->uuid));
    JS_SetPropertyStr(c, res, "name", JS_NewString(c, obj->name.c_str()));
    return res;
  }

  const JSCFunctionListEntry OBJ_PROTO[] = {
    JS_CGETSET_DEF("uuid", getUUID, nullptr),
    JS_CGETSET_DEF("name", getName, setName),
    JS_CGETSET_DEF("pos", getProp<&Project::Object::pos>, setProp<&Project::Object::pos>),
    JS_CGETSET_DEF("rot", getProp<&Project::Object::rot>, setProp<&Project::Object::rot>),
    JS_CGETSET_DEF("scale", getProp<&Project::Object::scale>, setProp<&Project::Object::scale>),
    JS_CFUNC_DEF("toJSON", 0, objToJSON),
  };
}

void Editor::ScriptHost::Impl::initObjects(JSContext *c)
{
  registerClass(c, objClassId, {.class_name = "SceneObject"}, OBJ_PROTO, std::size(OBJ_PROTO));
}

JSValue Editor::ScriptHost::Impl::getSelectedObjects(JSContext *c, JSValueConst, int, JSValueConst*)
{
  JSValue arr = JS_NewArray(c);
  uint32_t idx = 0;
  for(auto uuid : ctx.getSelectedObjectUUIDs()) {
    JS_SetPropertyUint32(c, arr, idx++, newObjectHandle(c, uuid));
  }
  return arr;
}
