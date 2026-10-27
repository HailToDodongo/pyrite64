/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "impl.h"

namespace
{
  JSClassID vecClassId{};

  struct QuatRef
  {
    Editor::ScriptHost::Impl::Quat::Resolver resolve{};
  };

  glm::quat* getVec(JSContext *c, JSValueConst self, bool forWrite = false)
  {
    auto *ref = (QuatRef*)JS_GetOpaque(self, vecClassId);
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
    JS_SetPropertyStr(c, res, "w", JS_NewFloat64(c, vec->w));
    return res;
  }

  JSValue normalize(JSContext *c, JSValueConst self, int, JSValueConst*)
  {
    auto *quat = getVec(c, self, true);
    if(!quat)return JS_EXCEPTION;
    *quat = glm::length(*quat) > 0.0f ? glm::normalize(*quat) : glm::identity<glm::quat>();
    return JS_DupValue(c, self);
  }

  const JSCFunctionListEntry QUAT_PROTO[] = {
    JS_CGETSET_DEF("x", getComp<0>, setComp<0>),
    JS_CGETSET_DEF("y", getComp<1>, setComp<1>),
    JS_CGETSET_DEF("z", getComp<2>, setComp<2>),
    JS_CGETSET_DEF("w", getComp<3>, setComp<3>),
    JS_CFUNC_DEF("normalize", 0, normalize),
    JS_CFUNC_DEF("toJSON", 0, toJSON),
  };
}

void Editor::ScriptHost::Impl::Quat::init(JSContext *c)
{
  registerClass(c, vecClassId, {
    .class_name = "Quat",
    .finalizer = [](JSRuntime*, JSValueConst val) {
      delete (QuatRef*)JS_GetOpaque(val, vecClassId);
    },
  }, QUAT_PROTO, std::size(QUAT_PROTO));
}

JSValue Editor::ScriptHost::Impl::Quat::create(JSContext *c, Resolver resolver)
{
  JSValue res = JS_NewObjectClass(c, vecClassId);
  JS_SetOpaque(res, new QuatRef{std::move(resolver)});
  return res;
}

bool Editor::ScriptHost::Impl::Quat::read(JSContext *c, JSValueConst val, glm::quat &out)
{
  for(uint32_t i=0; i<4; ++i) {
    const char name[2] = {"xyzw"[i], 0};
    JSValue item = JS_GetPropertyStr(c, val, name);
    double num = 0;
    bool isNum = JS_IsNumber(item) && JS_ToFloat64(c, &num, item) == 0;
    JS_FreeValue(c, item);
    if(!isNum)return false;
    out[i] = (float)num;
  }
  return true;
}
