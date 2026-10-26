/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "quickjs.h"

namespace Editor::ScriptHost::Impl
{
  // In JS: editor.addAction(id, name, callback)
  JSValue addAction(JSContext *c, JSValueConst thisVal, int argc, JSValueConst *argv);

   // Removes all actions registered by scripts, must be called before the context is freed.
  void clearActions(JSContext *c);
}
