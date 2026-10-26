/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "impl.h"
#include "../scriptHost.h"
#include "../scriptUtils.h"
#include "../../actions.h"
#include "../../../utils/logger.h"

#include <string>
#include <unordered_map>

namespace
{
  namespace Act = Editor::Actions;

  std::unordered_map<std::string, JSValue> scriptActions{};

  bool callScriptAction(const std::string &id, const std::string &arg)
  {
    auto &js = Editor::ScriptHost::getContext();
    auto it = scriptActions.find(id);
    if(it == scriptActions.end() || !js.ctx)return false;

    JSValue jsArg = JS_NewString(js.ctx, arg.c_str());
    JSValue res = JS_Call(js.ctx, it->second, JS_UNDEFINED, 1, &jsArg);
    JS_FreeValue(js.ctx, jsArg);

    bool ok = !JS_IsException(res);
    if(!ok) {
      JSValue ex = JS_GetException(js.ctx);
      auto errorStr = Editor::ScriptHost::toString(ex, js);
      Utils::Logger::log("Action '" + id + "' failed: " + errorStr, Utils::Logger::LEVEL_ERROR);
      JS_FreeValue(js.ctx, ex);
    }
    JS_FreeValue(js.ctx, res);
    return ok;
  }
}

JSValue Editor::ScriptHost::Impl::addAction(JSContext *c, JSValueConst, int argc, JSValueConst *argv)
{
  if(argc < 3 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]) || !JS_IsFunction(c, argv[2])) {
    return JS_ThrowTypeError(c, "addAction expects (id, name, callback)");
  }
  auto id = toString(argv[0], c);
  auto name = toString(argv[1], c);
  if(id.empty()) {
    return JS_ThrowTypeError(c, "action id must not be empty");
  }

  auto it = scriptActions.find(id);
  if(it != scriptActions.end()) {
    JS_FreeValue(c, it->second);
  } else {
    for(auto *action : Act::getActions()) {
      if(action->id == id)return JS_ThrowTypeError(c, "action '%s' already exists", id.c_str());
    }
  }

  scriptActions[id] = JS_DupValue(c, argv[2]);
  Act::registerAction(id, name, [id](const std::string &arg) {
    return callScriptAction(id, arg);
  });
  return JS_UNDEFINED;
}

void Editor::ScriptHost::Impl::clearActions(JSContext *c)
{
  for(auto &[id, fn] : scriptActions) {
    Act::unregisterAction(id);
    JS_FreeValue(c, fn);
  }
  scriptActions.clear();
}
