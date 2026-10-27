/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <functional>
#include "quickjs.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Editor::ScriptHost::Impl
{
  inline void registerClass(JSContext *c, JSClassID &id, const JSClassDef &def, const JSCFunctionListEntry *proto, int protoLen)
  {
    auto *rt = JS_GetRuntime(c);
    JS_NewClassID(rt, &id);
    JS_NewClass(rt, id, &def);
    JSValue protoObj = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, protoObj, proto, protoLen);
    JS_SetClassProto(c, id, protoObj);
  }

  //########// Actions //########//

  // In JS: editor.addAction(id, name, callback)
  JSValue addAction(JSContext *c, JSValueConst thisVal, int argc, JSValueConst *argv);

   // Removes all actions registered by scripts, must be called before the context is freed.
  void clearActions(JSContext *c);

  //########// Objects //########//

  // Registers the scene object class, must be called once per context
  void initObjects(JSContext *c);

  // In JS: editor.getSelectedObjects()
  JSValue getSelectedObjects(JSContext *c, JSValueConst thisVal, int argc, JSValueConst *argv);

  //########// Math Types //########//

  struct Vec3
  {
    // Returns the vector a handle points to, or nullptr + JS exception.
    // forWrite is true if the access is writing to it (handles prefab overrides)
    using Resolver = std::function<glm::vec3*(JSContext *c, bool forWrite)>;

    // Registers the vector class, must be called once per context
    static void init(JSContext *c);

    // Live vector, the resolver is called on each access
    static JSValue create(JSContext *c, Resolver resolver);

    // Accepts {x: 1, y: 2, z: 3} objects or another vector
    static bool read(JSContext *c, JSValueConst val, glm::vec3 &out);
  };

  struct Quat
  {
    using Resolver = std::function<glm::quat*(JSContext *c, bool forWrite)>;

    static void init(JSContext *c);
    static JSValue create(JSContext *c, Resolver resolver);
    static bool read(JSContext *c, JSValueConst val, glm::quat &out);
  };
}
