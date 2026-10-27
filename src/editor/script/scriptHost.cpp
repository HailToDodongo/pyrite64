/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scriptHost.h"
#include "../../utils/logger.h"
#include "../../utils/fs.h"
#include "../actions.h"
#include "../undoRedo.h"
#include "../../project/scene/object.h"
#include "../../project/scene/scene.h"
#include "../pages/editorScene.h"
#include "../../context.h"
#include "scriptUtils.h"
#include "impl/impl.h"
#include <algorithm>

namespace
{
  namespace Act = Editor::Actions;

  Editor::ScriptHost::Context js{};

  #define JS_LAMBDA(name, args, fn) JS_CFUNC_DEF(name, args, (\
    [](JSContext *c, JSValueConst thisVal, int argc, JSValueConst *argv) -> JSValue fn \
    )) \

  struct APIObjDef
  {
    const char* name;
    std::vector<JSCFunctionListEntry> entries{};
  };

  const std::array<APIObjDef, 4> APIs{
    APIObjDef
    {
      .name = "editor",
      .entries = {
        JS_LAMBDA("build", 1, {
          bool doRun = argc > 0 ? JS_ToBool(js.ctx, argv[0]) : false;
          bool res = Act::call(Act::Type::PROJECT_BUILD, doRun ? "run" : "");
          return JS_NewBool(c, res);
        }),
        JS_LAMBDA("save", 0, {
          if(ctx.project) {
            ctx.project->save();
            ctx.editorScene->save();
          }
          return JS_UNDEFINED;
        }),
        JS_LAMBDA("close", 0, { return JS_NewBool(c, Act::call(Act::Type::PROJECT_CLOSE)); }),
        JS_LAMBDA("clean", 0, { return JS_NewBool(c, Act::call(Act::Type::PROJECT_CLEAN)); }),
        JS_LAMBDA("reloadAssets", 0, { return JS_NewBool(c, Act::call(Act::Type::ASSETS_RELOAD)); }),
        JS_CFUNC_DEF("addAction", 3, Editor::ScriptHost::Impl::addAction),
        JS_CFUNC_DEF("getSelObjects", 0, Editor::ScriptHost::Impl::getSelectedObjects),
      }
    },{
      .name = "history",
      .entries = {
        JS_LAMBDA("markChanged", 1, {
          auto desc = argc > 0 ? toString(argv[0], js) : std::string{};
          Editor::UndoRedo::getHistory().markChanged(desc.empty() ? "Script Change" : desc);
          return JS_UNDEFINED;
        }),
        JS_LAMBDA("undo", 0, { return JS_NewBool(c, Editor::UndoRedo::getHistory().undo()); }),
        JS_LAMBDA("redo", 0, { return JS_NewBool(c, Editor::UndoRedo::getHistory().redo()); }),
      }
    },{
      .name = "console",
      .entries = {
        JS_LAMBDA("clear", 0, {
          Utils::Logger::clear();
          return JS_UNDEFINED;
        }),
        JS_LAMBDA("log", 1, {
          auto str = argc > 0 ? toString(argv[0], js) : std::string{};
          Utils::Logger::log(str);
          return JS_UNDEFINED;
        })
      }
    },{
      .name = "scene",
      .entries = {
        JS_LAMBDA("load", 1, {
          if(!ctx.project)return JS_FALSE;
          int id = 0;
          if(argc > 0 && JS_ToInt32(c, &id, argv[0]) != 0) {
            return JS_EXCEPTION;
          }
          if(id <= 0)return JS_FALSE;
          
          ctx.project->getScenes().loadScene(id);
          return JS_TRUE;
        }),
      }
    }
  };
}

namespace Editor::ScriptHost
{
  const Context& getContext() {
    return js;
  }

  bool start(Project::Project &project)
  {
    stop();

    js.rt = JS_NewRuntime();
    if(!js.rt) {
      Utils::Logger::log("Failed to create editor script runtime", Utils::Logger::LEVEL_ERROR);
      return false;
    }

    js.ctx = JS_NewContext(js.rt);
    if(!js.ctx) {
      Utils::Logger::log("Failed to create editor script context", Utils::Logger::LEVEL_ERROR);
      JS_FreeRuntime(js.rt);
      js.rt = nullptr;
      return false;
    }

    Impl::Vec3::init(js.ctx);
    Impl::Quat::init(js.ctx);
    Impl::initObjects(js.ctx);

    JSValue global = JS_GetGlobalObject(js.ctx);
    
    for(auto &def : APIs) {
      JSValue api = JS_NewObject(js.ctx);
      JS_SetPropertyFunctionList(js.ctx, api, def.entries.data(), def.entries.size());
      JS_SetPropertyStr(js.ctx, global, def.name, api);
    }

    JS_FreeValue(js.ctx, global);

    Utils::Logger::log("Editor scripts: context started");

    for(auto &entry : project.getAssets().getTypeEntries(Project::FileType::EDITOR_SCRIPT)) {
      if(!entry.conf.autoRun)continue;
      Utils::Logger::log("Auto-running script: " + entry.name);
      auto res = runFile(entry.path);
      if(!res.success)Utils::Logger::log(res.text, Utils::Logger::LEVEL_ERROR);
    }
    return true;
  }

  void stop()
  {
    if(!js.rt)return;

    Impl::clearActions(js.ctx);

    JS_FreeContext(js.ctx);
    JS_FreeRuntime(js.rt);
    js.ctx = nullptr;
    js.rt = nullptr;
    Utils::Logger::log("Editor scripts: context stopped");
  }

  bool isRunning() {
    return js.ctx != nullptr;
  }

  EvalResult eval(const std::string &code, const std::string &fileName)
  {
    if(!js.ctx)return {"No project loaded!", false};

    JSValue val = JS_Eval(js.ctx, code.c_str(), code.size(), fileName.c_str(), JS_EVAL_TYPE_GLOBAL);

    if(JS_IsException(val)) {
      JSValue ex = JS_GetException(js.ctx);
      std::string text = toString(ex, js);
      JSValue stack = JS_GetPropertyStr(js.ctx, ex, "stack");

      if(JS_IsString(stack)) {
        text += "\n" + toString(stack, js);
      }
      JS_FreeValue(js.ctx, stack);
      JS_FreeValue(js.ctx, ex);
      return {text, false};
    }

    EvalResult res{.success = true};
    if(JS_IsObject(val) && !JS_IsFunction(js.ctx, val)) {
      JSValue json = JS_JSONStringify(js.ctx, val, JS_UNDEFINED, JS_UNDEFINED);
      res.text = JS_IsException(json) ? toString(val, js) : toString(json, js);
      JS_FreeValue(js.ctx, json);
    } else {
      res.text = toString(val, js);
    }
    JS_FreeValue(js.ctx, val);
    return res;
  }

  EvalResult runFile(const std::string &path)
  {
    if(!fs::exists(path))return {"Script not found: " + path, false};
    return eval(Utils::FS::loadTextFile(path), fs::path{path}.filename().string());
  }
}
