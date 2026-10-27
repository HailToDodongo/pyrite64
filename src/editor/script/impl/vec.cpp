/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "impl.h"

namespace
{
  JSClassID vecClassId{};

  struct VecRef
  {
    Editor::ScriptHost::Impl::Vec3::Resolver resolve{};
  };

  glm::vec3* getVec(JSContext *c, JSValueConst self, bool forWrite = false)
  {
    auto *ref = (VecRef*)JS_GetOpaque(self, vecClassId);
    return ref ? ref->resolve(c, forWrite) : nullptr;
  }

  template<int IDX>
  JSValue getComp(JSContext *c, JSValueConst self)
  {
    auto *vec = getVec(c, self);
    return vec ? JS_NewFloat64(c, (*vec)[IDX]) : JS_EXCEPTION;
  }

  template<int IDX>
  JSValue setComp(JSContext *c, JSValueConst self, JSValueConst val)
  {
    auto *vec = getVec(c, self, true);
    if(!vec)return JS_EXCEPTION;
    double num = 0;
    if(!JS_IsNumber(val) || JS_ToFloat64(c, &num, val) != 0)return JS_ThrowTypeError(c, "expected a number");
    (*vec)[IDX] = (float)num;
    return JS_UNDEFINED;
  }

  JSValue toJSON(JSContext *c, JSValueConst self, int, JSValueConst*)
  {
    auto *vec = getVec(c, self);
    if(!vec)return JS_EXCEPTION;
    JSValue res = JS_NewObject(c);
    JS_SetPropertyStr(c, res, "x", JS_NewFloat64(c, vec->x));
    JS_SetPropertyStr(c, res, "y", JS_NewFloat64(c, vec->y));
    JS_SetPropertyStr(c, res, "z", JS_NewFloat64(c, vec->z));
    return res;
  }

  const JSCFunctionListEntry VEC_PROTO[] = {
    JS_CGETSET_DEF("x", getComp<0>, setComp<0>),
    JS_CGETSET_DEF("y", getComp<1>, setComp<1>),
    JS_CGETSET_DEF("z", getComp<2>, setComp<2>),
    JS_CFUNC_DEF("toJSON", 0, toJSON),
  };
}

void Editor::ScriptHost::Impl::Vec3::init(JSContext *c)
{
  registerClass(c, vecClassId, {
    .class_name = "Vec3",
    .finalizer = [](JSRuntime*, JSValueConst val) {
      delete (VecRef*)JS_GetOpaque(val, vecClassId);
    },
  }, VEC_PROTO, std::size(VEC_PROTO));
}

JSValue Editor::ScriptHost::Impl::Vec3::create(JSContext *c, Resolver resolver)
{
  JSValue res = JS_NewObjectClass(c, vecClassId);
  JS_SetOpaque(res, new VecRef{std::move(resolver)});
  return res;
}

bool Editor::ScriptHost::Impl::Vec3::read(JSContext *c, JSValueConst val, glm::vec3 &out)
{
  for(uint32_t i=0; i<3; ++i) {
    const char name[2] = {"xyz"[i], 0};
    JSValue item = JS_GetPropertyStr(c, val, name);
    double num = 0;
    bool isNum = JS_IsNumber(item) && JS_ToFloat64(c, &num, item) == 0;
    JS_FreeValue(c, item);
    if(!isNum)return false;
    out[i] = (float)num;
  }
  return true;
}
