/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "jsNodeHost.h"
#include "nodes/scriptNode.h"
#include "nodes/baseNode.h"
#include "valueTypes.h"
#include "../../utils/logger.h"
#include "../../utils/string.h"
#include "../../utils/hash.h"

#include "quickjs.h"

#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using Project::Graph::BuildCtx;
using namespace Project::Graph::Node;

namespace
{

  // ---- runtime state ----------------------------------------------------------
  JSRuntime* g_rt = nullptr;
  JSContext* g_jctx = nullptr;
  JSValue    g_ctxObj{};           // persistent BuildCtx-bound JS object
  bool       g_ready = false;
  std::recursive_mutex g_mutex{};  // QuickJS is single-threaded; serialize all access
  BuildCtx*  g_buildCtx = nullptr; // the (single) BuildCtx of the in-progress build

  // ---- value helpers ----------------------------------------------------------
  std::string jsStr(JSContext* c, JSValueConst v)
  {
    const char* s = JS_ToCString(c, v);
    std::string out = s ? s : "";
    if(s) JS_FreeCString(c, s);
    return out;
  }

  // JS value -> C++ code token: strings pass through, integral numbers print without
  // a decimal point, bools as 1/0.
  std::string jsToCode(JSContext* c, JSValueConst v)
  {
    if(JS_IsString(v)) return jsStr(c, v);
    if(JS_IsBool(v))   return JS_ToBool(c, v) ? "1" : "0";
    if(JS_IsNumber(v)) {
      double d = 0; JS_ToFloat64(c, &d, v);
      if(d == (double)(int64_t)d) return std::to_string((int64_t)d);
      return std::to_string(d);
    }
    return jsStr(c, v);
  }

  std::string jsException()
  {
    JSValue e = JS_GetException(g_jctx);
    std::string msg = jsStr(g_jctx, e);
    JSValue st = JS_GetPropertyStr(g_jctx, e, "stack");
    if(JS_IsString(st)) { msg += "\n"; msg += jsStr(g_jctx, st); }
    JS_FreeValue(g_jctx, st);
    JS_FreeValue(g_jctx, e);
    return msg;
  }

  JSValue jsonToJs(JSContext* c, const nlohmann::json &j)
  {
    using nlohmann::json;
    switch(j.type()) {
      case json::value_t::boolean:         return JS_NewBool(c, j.get<bool>());
      case json::value_t::number_integer:  return JS_NewInt64(c, j.get<int64_t>());
      case json::value_t::number_unsigned: return JS_NewInt64(c, (int64_t)j.get<uint64_t>());
      case json::value_t::number_float:    return JS_NewFloat64(c, j.get<double>());
      case json::value_t::string:          return JS_NewString(c, j.get<std::string>().c_str());
      case json::value_t::array: {
        JSValue a = JS_NewArray(c); uint32_t i = 0;
        for(auto &e : j) JS_SetPropertyUint32(c, a, i++, jsonToJs(c, e));
        return a;
      }
      case json::value_t::object: {
        JSValue o = JS_NewObject(c);
        for(auto &[k, v] : j.items()) JS_SetPropertyStr(c, o, k.c_str(), jsonToJs(c, v));
        return o;
      }
      default: return JS_NULL;
    }
  }

  // ---- BuildCtx methods exposed to JS (operate on g_buildCtx) ------------------
  // Statement-emitting methods return `this` so calls chain; value methods return a value.
  JSValue js_line(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 1) g_buildCtx->line(jsStr(c, argv[0]));
    return JS_DupValue(c, self);
  }
  JSValue js_local_const(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 3) g_buildCtx->localConst(jsStr(c,argv[0]), jsStr(c,argv[1]), jsToCode(c,argv[2]));
    return JS_DupValue(c, self);
  }
  JSValue js_local_var(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 3) g_buildCtx->localVar(jsStr(c,argv[0]), jsStr(c,argv[1]), jsToCode(c,argv[2]));
    return JS_DupValue(c, self);
  }
  JSValue js_set_var(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 2) g_buildCtx->setVar(jsStr(c,argv[0]), jsToCode(c,argv[1]));
    return JS_DupValue(c, self);
  }
  JSValue js_incr_var(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 2) g_buildCtx->incrVar(jsStr(c,argv[0]), jsToCode(c,argv[1]));
    return JS_DupValue(c, self);
  }
  JSValue js_global_var(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(!g_buildCtx) return JS_DupValue(c, self);
    // (type, value) -> auto-named, returns the name; (type, name, value) -> chains.
    if(argc >= 3) { g_buildCtx->globalVar(jsStr(c,argv[0]), jsStr(c,argv[1]), jsToCode(c,argv[2])); return JS_DupValue(c, self); }
    if(argc >= 2) return JS_NewString(c, g_buildCtx->globalVar(jsStr(c,argv[0]), jsToCode(c,argv[1])).c_str());
    return JS_DupValue(c, self);
  }
  JSValue js_declare_var(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 3) g_buildCtx->declareVar(jsStr(c,argv[0]), jsStr(c,argv[1]), jsToCode(c,argv[2]));
    return JS_DupValue(c, self);
  }
  JSValue js_jump(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 1) { int32_t i = 0; JS_ToInt32(c, &i, argv[0]); g_buildCtx->jump((uint32_t)i); }
    return JS_DupValue(c, self);
  }
  JSValue js_include(JSContext* c, JSValueConst self, int argc, JSValueConst* argv) {
    if(g_buildCtx && argc >= 1) g_buildCtx->include(jsStr(c, argv[0]));
    return JS_DupValue(c, self);
  }
  JSValue js_input_expr(JSContext* c, JSValueConst, int argc, JSValueConst* argv) {
    if(!g_buildCtx || argc < 1) return JS_NewString(c, "0");
    int32_t i = 0; JS_ToInt32(c, &i, argv[0]);
    // Omitting the fallback lets inputExpr supply a typed zero (e.g. "fm_vec3_t{}").
    std::string fb = argc >= 2 ? jsStr(c, argv[1]) : "";
    return JS_NewString(c, g_buildCtx->inputExpr((size_t)i, fb).c_str());
  }
  JSValue js_has_value_input(JSContext* c, JSValueConst, int argc, JSValueConst* argv) {
    if(!g_buildCtx || argc < 1) return JS_NewBool(c, false);
    int32_t i = 0; JS_ToInt32(c, &i, argv[0]);
    return JS_NewBool(c, g_buildCtx->hasValueInput((size_t)i));
  }
  JSValue js_crc32(JSContext* c, JSValueConst, int argc, JSValueConst* argv) {
    if(argc < 1) return JS_NewInt64(c, 0);
    return JS_NewInt64(c, (int64_t)Utils::Hash::crc32(jsStr(c, argv[0])));
  }

  void setFn(JSContext* c, JSValue obj, const char* name, JSCFunction* fn, int len) {
    JS_SetPropertyStr(c, obj, name, JS_NewCFunction(c, fn, name, len));
  }

  std::string readFile(const std::string &path) {
    std::ifstream f(path); std::stringstream ss; ss << f.rdbuf(); return ss.str();
  }

  bool evalGlobal(const std::string &src, const std::string &file) {
    JSValue r = JS_Eval(g_jctx, src.c_str(), src.size(), file.c_str(), JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(r);
    if(!ok) Utils::Logger::log("JS error in " + file + ": " + jsException(), Utils::Logger::LEVEL_ERROR);
    JS_FreeValue(g_jctx, r);
    return ok;
  }

  // ---- reading a node description object back into a NodeSpec ------------------
  PropType parsePropType(const std::string &t) {
    if(t == "u16")     return PropType::U16;
    if(t == "u32")     return PropType::U32;
    if(t == "i32")     return PropType::I32;
    if(t == "f32")     return PropType::F32;
    if(t == "bool")    return PropType::Bool;
    if(t == "string")  return PropType::String;
    if(t == "enum")    return PropType::Enum;
    if(t == "sceneref")return PropType::SceneRef;
    return PropType::I32;
  }

  std::string getStr(JSContext* c, JSValueConst o, const char* k, const std::string &def = "") {
    JSValue v = JS_GetPropertyStr(c, o, k);
    std::string r = JS_IsString(v) ? jsStr(c, v) : def;
    JS_FreeValue(c, v);
    return r;
  }
  double getNum(JSContext* c, JSValueConst o, const char* k, double def) {
    JSValue v = JS_GetPropertyStr(c, o, k);
    double r = def; if(JS_IsNumber(v)) JS_ToFloat64(c, &r, v);
    JS_FreeValue(c, v);
    return r;
  }
  bool getBool(JSContext* c, JSValueConst o, const char* k) {
    JSValue v = JS_GetPropertyStr(c, o, k);
    bool r = JS_ToBool(c, v) > 0;
    JS_FreeValue(c, v);
    return r;
  }
  uint32_t jsArrayLen(JSContext* c, JSValueConst a) {
    JSValue l = JS_GetPropertyStr(c, a, "length");
    int32_t n = 0; JS_ToInt32(c, &n, l);
    JS_FreeValue(c, l);
    return (uint32_t)n;
  }

  // forward decls for the codegen bridges captured by spec lambdas
  void callBuild(const std::string &id, ScriptNode &n, BuildCtx &ctx);
  std::string callValue(const std::string &id, ScriptNode &n, BuildCtx &ctx);

  std::vector<PinDef> readPins(JSContext* c, JSValueConst d, const char* key) {
    std::vector<PinDef> pins{};
    JSValue arr = JS_GetPropertyStr(c, d, key);
    if(JS_IsArray(arr)) {
      uint32_t n = jsArrayLen(c, arr);
      for(uint32_t i = 0; i < n; ++i) {
        JSValue p = JS_GetPropertyUint32(c, arr, i);
        pins.push_back({ getStr(c, p, "name"), getBool(c, p, "value"), getStr(c, p, "type"),
                         (float)getNum(c, p, "default", 0.0) });
        JS_FreeValue(c, p);
      }
    }
    JS_FreeValue(c, arr);
    return pins;
  }

  NodeSpec specFromJs(JSContext* c, JSValueConst d)
  {
    NodeSpec spec{};
    spec.id       = getStr(c, d, "id");
    spec.name     = getStr(c, d, "name");
    spec.category = getStr(c, d, "category");
    spec.entry    = getBool(c, d, "entry");
    spec.hidden   = getBool(c, d, "hidden");

    { JSValue r = JS_GetPropertyStr(c, d, "rounding");
      if(JS_IsNumber(r)) { double v = 0; JS_ToFloat64(c, &v, r); spec.rounding = (float)v; }
      JS_FreeValue(c, r); }

    { JSValue t = JS_GetPropertyStr(c, d, "title");
      if(JS_IsString(t)) spec.titleTemplate = jsStr(c, t);
      JS_FreeValue(c, t); }

    { JSValue col = JS_GetPropertyStr(c, d, "color");
      if(JS_IsArray(col)) {
        auto comp = [&](uint32_t i, int def){
          JSValue e = JS_GetPropertyUint32(c, col, i);
          int32_t v = def; if(JS_IsNumber(e)) JS_ToInt32(c, &v, e);
          JS_FreeValue(c, e); return v;
        };
        int a = jsArrayLen(c, col) > 3 ? comp(3, 255) : 255;
        spec.color = IM_COL32(comp(0,90), comp(1,191), comp(2,93), a);
      }
      JS_FreeValue(c, col); }

    spec.inputs  = readPins(c, d, "inputs");
    spec.outputs = readPins(c, d, "outputs");

    { JSValue arr = JS_GetPropertyStr(c, d, "props");
      if(JS_IsArray(arr)) {
        uint32_t n = jsArrayLen(c, arr);
        for(uint32_t i = 0; i < n; ++i) {
          JSValue p = JS_GetPropertyUint32(c, arr, i);
          PropDef prop{};
          prop.key   = getStr(c, p, "key");
          prop.label = getStr(c, p, "label");
          prop.type  = parsePropType(getStr(c, p, "type"));
          prop.width = (float)getNum(c, p, "width", 80.0);
          { JSValue h = JS_GetPropertyStr(c, p, "hideIfInputConnected");
            if(JS_IsNumber(h)) { int32_t v = -1; JS_ToInt32(c, &v, h); prop.hideIfInputConnected = v; }
            JS_FreeValue(c, h); }
          prop.compact = getBool(c, p, "compact");
          { JSValue dv = JS_GetPropertyStr(c, p, "default");
            if(prop.type == PropType::String) { if(JS_IsString(dv)) prop.defStr = jsStr(c, dv); }
            else if(JS_IsNumber(dv)) { double v = 0; JS_ToFloat64(c, &v, dv); prop.defNum = (float)v; }
            JS_FreeValue(c, dv); }
          { JSValue en = JS_GetPropertyStr(c, p, "enum");
            if(JS_IsArray(en)) {
              uint32_t m = jsArrayLen(c, en);
              for(uint32_t k = 0; k < m; ++k) {
                JSValue e = JS_GetPropertyUint32(c, en, k);
                prop.enumOptions.push_back(jsStr(c, e));
                JS_FreeValue(c, e);
              }
            }
            JS_FreeValue(c, en); }
          spec.props.push_back(std::move(prop));
          JS_FreeValue(c, p);
        }
      }
      JS_FreeValue(c, arr); }

    std::string id = spec.id;
    if(getBool(c, d, "hasBuild")) spec.build = [id](ScriptNode &n, BuildCtx &ctx){ callBuild(id, n, ctx); };
    if(getBool(c, d, "hasValue")) spec.value = [id](ScriptNode &n, BuildCtx &ctx){ return callValue(id, n, ctx); };
    return spec;
  }

  // ---- codegen bridges (JS build/value), serialized + reentrant ---------------
  void callBuild(const std::string &id, ScriptNode &n, BuildCtx &ctx)
  {
    std::lock_guard<std::recursive_mutex> lk(g_mutex);
    if(!g_ready)return;
    g_buildCtx = &ctx;
    std::string resVar = "res_" + Utils::toHex64(n.uuid);
    JSValue global = JS_GetGlobalObject(g_jctx);
    JSValue fn = JS_GetPropertyStr(g_jctx, global, "__invoke_build");
    JSValue args[4] = {
      JS_NewString(g_jctx, id.c_str()),
      jsonToJs(g_jctx, n.getProps()),
      JS_DupValue(g_jctx, g_ctxObj),
      JS_NewString(g_jctx, resVar.c_str()),
    };
    JSValue r = JS_Call(g_jctx, fn, JS_UNDEFINED, 4, args);
    if(JS_IsException(r)) Utils::Logger::log("Node '" + id + "' build error: " + jsException(), Utils::Logger::LEVEL_ERROR);
    for(auto &a : args) JS_FreeValue(g_jctx, a);
    JS_FreeValue(g_jctx, r);
    JS_FreeValue(g_jctx, fn);
    JS_FreeValue(g_jctx, global);
  }

  std::string callValue(const std::string &id, ScriptNode &n, BuildCtx &ctx)
  {
    std::lock_guard<std::recursive_mutex> lk(g_mutex);
    if(!g_ready)return "0";
    g_buildCtx = &ctx;
    std::string resVar = "res_" + Utils::toHex64(n.uuid);
    JSValue global = JS_GetGlobalObject(g_jctx);
    JSValue fn = JS_GetPropertyStr(g_jctx, global, "__invoke_value");
    JSValue args[4] = {
      JS_NewString(g_jctx, id.c_str()),
      jsonToJs(g_jctx, n.getProps()),
      JS_DupValue(g_jctx, g_ctxObj),
      JS_NewString(g_jctx, resVar.c_str()),
    };
    JSValue r = JS_Call(g_jctx, fn, JS_UNDEFINED, 4, args);
    std::string out = "0";
    if(JS_IsException(r)) Utils::Logger::log("Node '" + id + "' value error: " + jsException(), Utils::Logger::LEVEL_ERROR);
    else if(JS_IsString(r)) out = jsStr(g_jctx, r);
    for(auto &a : args) JS_FreeValue(g_jctx, a);
    JS_FreeValue(g_jctx, r);
    JS_FreeValue(g_jctx, fn);
    JS_FreeValue(g_jctx, global);
    return out;
  }

  void evalDir(const std::string &dir)
  {
    if(dir.empty() || !fs::is_directory(dir))return;
    std::vector<fs::path> files{};
    for(auto &e : fs::directory_iterator(dir)) {
      if(e.is_regular_file() && e.path().extension() == ".js") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    for(auto &p : files) {
      // wrap in an IIFE so each script's top-level names stay local.
      std::string wrapped = "(()=>{\n" + readFile(p.string()) + "\n})();";
      evalGlobal(wrapped, p.string());
    }
  }

  void callGlobalVoid(const char* name)
  {
    JSValue global = JS_GetGlobalObject(g_jctx);
    JSValue fn = JS_GetPropertyStr(g_jctx, global, name);
    if(JS_IsFunction(g_jctx, fn)) {
      JSValue r = JS_Call(g_jctx, fn, JS_UNDEFINED, 0, nullptr);
      JS_FreeValue(g_jctx, r);
    }
    JS_FreeValue(g_jctx, fn);
    JS_FreeValue(g_jctx, global);
  }

  ImU32 readColor(JSContext* c, JSValueConst o, const char* k)
  {
    JSValue col = JS_GetPropertyStr(c, o, k);
    ImU32 out = IM_COL32(0xFF, 0x99, 0x55, 0xFF);
    if(JS_IsArray(col)) {
      auto comp = [&](uint32_t i, int def){
        JSValue e = JS_GetPropertyUint32(c, col, i);
        int32_t v = def; if(JS_IsNumber(e)) JS_ToInt32(c, &v, e);
        JS_FreeValue(c, e); return v;
      };
      out = IM_COL32(comp(0,0xFF), comp(1,0x99), comp(2,0x55), 0xFF);
    }
    JS_FreeValue(c, col);
    return out;
  }

  // Reads the value-type table (data/nodes/_types.js) into the C++ registry.
  void readValueTypes()
  {
    callGlobalVoid("__resetTypes");
    clearValueTypes();
    evalGlobal(readFile("data/nodes/_types.js"), "_types.js");

    JSValue global = JS_GetGlobalObject(g_jctx);
    JSValue fn = JS_GetPropertyStr(g_jctx, global, "__describeTypes");
    JSValue desc = JS_Call(g_jctx, fn, JS_UNDEFINED, 0, nullptr);
    if(JS_IsException(desc)) {
      Utils::Logger::log(std::string("Loading value types failed: ") + jsException(), Utils::Logger::LEVEL_ERROR);
    } else {
      JSValue types = JS_GetPropertyStr(g_jctx, desc, "types");
      if(JS_IsArray(types)) {
        uint32_t n = jsArrayLen(g_jctx, types);
        for(uint32_t i = 0; i < n; ++i) {
          JSValue t = JS_GetPropertyUint32(g_jctx, types, i);
          ValueType vt{};
          vt.id             = getStr(g_jctx, t, "id");
          vt.name           = getStr(g_jctx, t, "name");
          vt.cType          = getStr(g_jctx, t, "cType", "int");
          vt.defaultLiteral = getStr(g_jctx, t, "default", "0");
          vt.color          = readColor(g_jctx, t, "color");
          vt.size           = (int)getNum(g_jctx, t, "size", 4.0);
          addValueType(vt);
          JS_FreeValue(g_jctx, t);
        }
      }
      JS_FreeValue(g_jctx, types);

      JSValue convs = JS_GetPropertyStr(g_jctx, desc, "conversions");
      if(JS_IsArray(convs)) {
        uint32_t n = jsArrayLen(g_jctx, convs);
        for(uint32_t i = 0; i < n; ++i) {
          JSValue cv = JS_GetPropertyUint32(g_jctx, convs, i);
          addConversion(getStr(g_jctx, cv, "from"), getStr(g_jctx, cv, "to"), getStr(g_jctx, cv, "tmpl"));
          JS_FreeValue(g_jctx, cv);
        }
      }
      JS_FreeValue(g_jctx, convs);
    }
    JS_FreeValue(g_jctx, desc);
    JS_FreeValue(g_jctx, fn);
    JS_FreeValue(g_jctx, global);
  }
}

namespace Project::Graph::Js
{
  bool init()
  {
    std::lock_guard<std::recursive_mutex> lk(g_mutex);
    if(g_ready)return true;

    g_rt = JS_NewRuntime();
    if(!g_rt)return false;
    // Disable the stack-overflow check: its init-time stack top breaks the build worker thread.
    JS_SetMaxStackSize(g_rt, 0);
    g_jctx = JS_NewContext(g_rt);
    if(!g_jctx) { JS_FreeRuntime(g_rt); g_rt = nullptr; return false; }

    JSValue global = JS_GetGlobalObject(g_jctx);
    setFn(g_jctx, global, "__crc32", js_crc32, 1);
    JS_FreeValue(g_jctx, global);

    if(!evalGlobal(readFile("data/nodes/_icons.js"), "_icons.js")) {
      return false;
    }

    g_ctxObj = JS_NewObject(g_jctx);
    setFn(g_jctx, g_ctxObj, "line",          js_line, 1);
    setFn(g_jctx, g_ctxObj, "localConst",    js_local_const, 3);
    setFn(g_jctx, g_ctxObj, "localVar",      js_local_var, 3);
    setFn(g_jctx, g_ctxObj, "setVar",        js_set_var, 2);
    setFn(g_jctx, g_ctxObj, "incrVar",       js_incr_var, 2);
    setFn(g_jctx, g_ctxObj, "globalVar",     js_global_var, 3);
    setFn(g_jctx, g_ctxObj, "declareVar",    js_declare_var, 3);
    setFn(g_jctx, g_ctxObj, "jump",          js_jump, 1);
    setFn(g_jctx, g_ctxObj, "include",       js_include, 1);
    setFn(g_jctx, g_ctxObj, "inputExpr",     js_input_expr, 2);
    setFn(g_jctx, g_ctxObj, "hasValueInput", js_has_value_input, 1);

    if(!evalGlobal(readFile("data/nodes/_prelude.js"), "_prelude.js")) {
      return false;
    }
    g_ready = true;
    return true;
  }

  void loadSpecs(std::vector<NodeSpec> &out, const std::string &userDir)
  {
    std::lock_guard<std::recursive_mutex> lk(g_mutex);
    if(!g_ready)return;

    readValueTypes();

    callGlobalVoid("__reset");
    evalDir("data/nodes/builtin");
    if(!userDir.empty()) evalDir(userDir);

    JSValue global = JS_GetGlobalObject(g_jctx);
    JSValue fn = JS_GetPropertyStr(g_jctx, global, "__describe");
    JSValue descs = JS_Call(g_jctx, fn, JS_UNDEFINED, 0, nullptr);
    if(JS_IsException(descs)) {
      Utils::Logger::log(std::string("Loading JS nodes failed: ") + jsException(), Utils::Logger::LEVEL_ERROR);
    } else if(JS_IsArray(descs)) {
      uint32_t n = jsArrayLen(g_jctx, descs);
      for(uint32_t i = 0; i < n; ++i) {
        JSValue d = JS_GetPropertyUint32(g_jctx, descs, i);
        out.push_back(specFromJs(g_jctx, d));
        JS_FreeValue(g_jctx, d);
      }
    }
    JS_FreeValue(g_jctx, descs);
    JS_FreeValue(g_jctx, fn);
    JS_FreeValue(g_jctx, global);
  }
}
