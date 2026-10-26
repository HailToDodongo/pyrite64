/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <quickjs.h>

namespace Editor::ScriptHost
{
  struct Context
  {
    JSRuntime* rt{};
    JSContext* ctx{};
  };

  inline std::string toString(JSValueConst val, JSContext* ctx)
  {
    const char* str = JS_ToCString(ctx, val);
    std::string res = str ? str : "";
    if(str)JS_FreeCString(ctx, str);
    return res;
  }

  inline std::string toString(JSValueConst val, const Context &js) {
    return toString(val, js.ctx);
  }
}