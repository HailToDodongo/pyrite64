/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "impl.h"
#include "../../../context.h"
#include "../../../project/scene/object.h"
#include "../../../project/scene/prefab.h"

#include <cstring>

using namespace Editor::ScriptHost::Impl;

namespace
{
  JSClassID compClassId{};

  struct CompRef
  {
    uint32_t objUUID{};
    uint64_t compUUID{};
  };

  Project::Object* getPrefabSource(Project::Object &obj)
  {
    if(!obj.isPrefabInstance())return nullptr;
    auto prefab = ctx.project->getAssets().getPrefabByUUID(obj.uuidPrefab.value);
    return prefab ? &prefab->obj : nullptr;
  }

  // components can be on the object itself or come from its prefab
  Project::Component::Entry* findComp(JSContext *c, JSValueConst self, Project::Object **outObj = nullptr)
  {
    auto *ref = (CompRef*)JS_GetOpaque(self, compClassId);
    auto *obj = ref ? findObject(c, ref->objUUID) : nullptr;
    if(!obj)return nullptr;
    if(outObj)*outObj = obj;

    for(auto *src : {obj, getPrefabSource(*obj)}) {
      if(!src)continue;
      for(auto &comp : src->components) {
        if(comp.uuid == ref->compUUID)return &comp;
      }
    }
    JS_ThrowReferenceError(c, "component no longer exists");
    return nullptr;
  }

  // prefab components first, same order as in the inspector
  template<typename F>
  void forEachComp(Project::Object &obj, F &&fn)
  {
    for(auto *src : {getPrefabSource(obj), &obj}) {
      if(!src)continue;
      for(auto &comp : src->components)fn(comp);
    }
  }

  JSValue newCompHandle(JSContext *c, uint32_t objUUID, uint64_t compUUID)
  {
    JSValue res = JS_NewObjectClass(c, compClassId);
    JS_SetOpaque(res, new CompRef{objUUID, compUUID});
    return res;
  }

  JSValue getUUID(JSContext *c, JSValueConst self)
  {
    auto *comp = findComp(c, self);
    return comp ? JS_NewString(c, std::to_string(comp->uuid).c_str()) : JS_EXCEPTION;
  }

  JSValue getType(JSContext *c, JSValueConst self)
  {
    auto *comp = findComp(c, self);
    return comp ? JS_NewString(c, Project::Component::TABLE[comp->id].name) : JS_EXCEPTION;
  }

  JSValue getName(JSContext *c, JSValueConst self)
  {
    auto *comp = findComp(c, self);
    return comp ? JS_NewString(c, comp->name.c_str()) : JS_EXCEPTION;
  }

  JSValue getEnabled(JSContext *c, JSValueConst self)
  {
    Project::Object *obj{};
    auto *comp = findComp(c, self, &obj);
    if(!comp)return JS_EXCEPTION;
    PropScope::Dispatch scope{obj->propOverrides, comp->uuid};
    return JS_NewBool(c, comp->enabled.resolve(*obj));
  }

  // same as the inspector, prefab instances write an override instead of the base value
  JSValue setEnabled(JSContext *c, JSValueConst self, JSValueConst val)
  {
    Project::Object *obj{};
    auto *comp = findComp(c, self, &obj);
    if(!comp)return JS_EXCEPTION;

    bool enabled = JS_ToBool(c, val);
    PropScope::Dispatch scope{obj->propOverrides, comp->uuid};
    if(obj->isPrefabInstance() && !ctx.isPrefabEditing(obj->uuid)) {
      if(!obj->hasPropOverride(comp->enabled))obj->addPropOverride(comp->enabled);
      comp->enabled.resolve(*obj) = enabled;
    } else {
      comp->enabled.value = enabled;
    }
    return JS_UNDEFINED;
  }

  JSValue toJSON(JSContext *c, JSValueConst self, int, JSValueConst*)
  {
    auto *comp = findComp(c, self);
    if(!comp)return JS_EXCEPTION;
    JSValue res = JS_NewObject(c);
    JS_SetPropertyStr(c, res, "type", JS_NewString(c, Project::Component::TABLE[comp->id].name));
    JS_SetPropertyStr(c, res, "name", JS_NewString(c, comp->name.c_str()));
    return res;
  }

  const JSCFunctionListEntry COMP_PROTO[] = {
    JS_CGETSET_DEF("uuid", getUUID, nullptr),
    JS_CGETSET_DEF("type", getType, nullptr),
    JS_CGETSET_DEF("name", getName, nullptr),
    JS_CGETSET_DEF("enabled", getEnabled, setEnabled),
    JS_CFUNC_DEF("toJSON", 0, toJSON),
  };
}

void Editor::ScriptHost::Impl::initComponents(JSContext *c)
{
  registerClass(c, compClassId, {
    .class_name = "Component",
    .finalizer = [](JSRuntime*, JSValueConst val) {
      delete (CompRef*)JS_GetOpaque(val, compClassId);
    },
  }, COMP_PROTO, std::size(COMP_PROTO));
}

JSValue Editor::ScriptHost::Impl::getComponents(JSContext *c, Project::Object &obj)
{
  JSValue arr = JS_NewArray(c);
  uint32_t idx = 0;
  forEachComp(obj, [&](auto &comp) {
    JS_SetPropertyUint32(c, arr, idx++, newCompHandle(c, obj.uuid, comp.uuid));
  });
  return arr;
}

JSValue Editor::ScriptHost::Impl::findComponent(JSContext *c, Project::Object &obj, const char* type)
{
  JSValue res = JS_NULL;
  forEachComp(obj, [&](auto &comp) {
    if(JS_IsNull(res) && type && strcmp(Project::Component::TABLE[comp.id].name, type) == 0) {
      res = newCompHandle(c, obj.uuid, comp.uuid);
    }
  });
  return res;
}
