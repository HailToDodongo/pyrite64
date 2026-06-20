/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include <functional>
#include <algorithm>
#include "ImNodeFlow.h"
#include "json.hpp"
#include "IconsMaterialDesignIcons.h"
#include "../../../utils/string.h"
#include "../valueTypes.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace Project::Graph
{
  struct BuildCtx
  {
    struct VarDef
    {
      std::string type{};
      std::string name{};
      std::string value{};
    };

    std::string source{};
    std::vector<VarDef> vars{};
    // Extra #include directives a node needs (e.g. for a custom value type's C++ header).
    // Each entry is the text after "#include ", e.g. "<myType.h>".
    std::vector<std::string> includes{};
    std::vector<uint64_t> *outUUIDs{nullptr};
    std::vector<uint64_t> *inValUUIDs{nullptr};
    // Dead-end output: "return;", or for a repeatable graph yield + goto Start.
    std::string flowEnd{"return;"};
    // Value-input type ids / inline-field literals (value-pin order).
    std::vector<std::string> *inValTypes{nullptr};
    std::vector<std::string> *inValFallbacks{nullptr};

    // Back-propagation resolvers, set by Graph::build.
    std::function<std::string(uint64_t)> valueResolver{};
    std::function<std::string(uint64_t)> valueTypeResolver{};
    // Graph variables (Set/Get nodes): lvalue expression and type id by name.
    std::function<std::string(const std::string&)> varLValue{};
    std::function<std::string(const std::string&)> varTypeOf{};

    bool hasValueInput(size_t i) const {
      return inValUUIDs && i < inValUUIDs->size() && (*inValUUIDs)[i] != 0;
    }

    std::string inputType(size_t i) const {
      return (inValTypes && i < inValTypes->size()) ? (*inValTypes)[i] : std::string{};
    }
    std::string inputCType(size_t i) const {
      return Node::cTypeOf(inputType(i));
    }

    // Resolves value input 'i' to a C++ expression (converted to its type). When
    // unconnected: 'fallback', else its inline-field literal, else a typed zero.
    std::string inputExpr(size_t i, const std::string &fallback = "") {
      if(hasValueInput(i) && valueResolver) {
        uint64_t producer = (*inValUUIDs)[i];
        std::string expr = valueResolver(producer);
        std::string from = valueTypeResolver ? valueTypeResolver(producer) : std::string{};
        return Node::convertExpr(from, inputType(i), expr);
      }
      if(!fallback.empty())return fallback;

      if(inValFallbacks && i < inValFallbacks->size() && !(*inValFallbacks)[i].empty()) {
        return (*inValFallbacks)[i];
      }
      std::string t = inputType(i);
      return t.empty() ? std::string{"0"} : (Node::cTypeOf(t) + "{}");
    }

    inline std::string toStr(auto value)
    {
      std::string valStr;
      if constexpr (std::is_same_v<decltype(value), std::string>) {
        return value;
      } else {
        return std::to_string(value);
      }
    }

    BuildCtx& localConst(const std::string &type, const std::string &varName, auto value) {
      source += "    constexpr "+type+" " + varName + " = " + toStr(value) + ";\n";
      return *this;
    }

    BuildCtx& localVar(const std::string &type, const std::string &varName, auto value) {
      source += "    "+type+" " + varName + " = " + toStr(value) + ";\n";
      return *this;
    }

    BuildCtx& setVar(const std::string &varName, auto value)
    {
      source += "    " + varName + " = " + toStr(value) + ";\n";
      return *this;
    }

    BuildCtx& incrVar(const std::string &varName, auto value)
    {
      source += "    " + varName + " += " + toStr(value) + ";\n";
      return *this;
    }

    BuildCtx& globalVar(const std::string &type, const std::string &name, auto initVal)
    {
      vars.push_back(VarDef{type, name, toStr(initVal)});
      return *this;
    }

    std::string globalVar(const std::string &type, auto initVal) {
      std::string varName = "gv_" + std::to_string(vars.size());
      globalVar(type, varName, initVal);
      return varName;
    }

    // Declares a persistent variable once; duplicate names are ignored.
    BuildCtx& declareVar(const std::string &type, const std::string &name, auto initVal) {
      for(const auto &v : vars) if(v.name == name) return *this;
      vars.push_back(VarDef{type, name, toStr(initVal)});
      return *this;
    }

    BuildCtx& jump(uint32_t outIndex) {
      // An unconnected (or out-of-range) output ends this execution path (flowEnd).
      uint64_t uuidOut = (outUUIDs && outIndex < outUUIDs->size()) ? (*outUUIDs)[outIndex] : 0;
      if(uuidOut) {
        source += "    goto NODE_" + Utils::toHex64(uuidOut) + ";\n";
      } else {
        source += "    " + flowEnd + "\n";
      }
      return *this;
    }

    BuildCtx& line(const std::string &str) {
      source += "    " + str + "\n";
      return *this;
    }

    BuildCtx& include(const std::string &path) {
      if(std::find(includes.begin(), includes.end(), path) == includes.end())
        includes.push_back(path);
      return *this;
    }
  };
}

namespace Project::Graph::Node
{
  extern std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_LOGIC;
  extern std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_VALUE;

  struct TypeLogic { };
  struct TypeValue { };

  class Base : public ImFlow::BaseNode
  {
    public:
      uint64_t uuid{};
      // Per-pin value-type ids in pin order, logic pins use Node::LOGIC_TYPE.
      std::vector<std::string> inTypes{};
      std::vector<std::string> outTypes{};

      // Whether the input pin at overall index 'i' is a value pin.
      bool isValueInput(size_t i) const {
        return i < inTypes.size() && inTypes[i] != "logic";
      }

      // Type ids of value inputs only, in value-pin order.
      std::vector<std::string> valueInputTypes() const {
        std::vector<std::string> out{};
        for(const auto &t : inTypes) if(t != "logic") out.push_back(t);
        return out;
      }

      // Type id of this node's first value output (its back-propagated value).
      std::string firstValueOutType() const {
        for(const auto &t : outTypes) if(t != "logic") return t;
        return {};
      }

      // Value-type id of an output pin (by pointer); empty if not found.
      std::string outPinType(const ImFlow::Pin* p) {
        auto &outs = getOuts();
        for(size_t i = 0; i < outs.size(); ++i) {
          if(outs[i].get() == p) return i < outTypes.size() ? outTypes[i] : std::string{};
        }
        return {};
      }

      // Value-type id of an input pin (by pointer); empty if not found.
      std::string inPinType(const ImFlow::Pin* p) {
        auto &ins = getIns();
        for(size_t i = 0; i < ins.size(); ++i) {
          if(ins[i].get() == p) return i < inTypes.size() ? inTypes[i] : std::string{};
        }
        return {};
      }

      virtual void serialize(nlohmann::json &j) = 0;
      virtual void deserialize(nlohmann::json &j) = 0;
      virtual void build(BuildCtx &ctx) = 0;
      // Resolve dynamic pin types from the building graph before codegen (no-op by default).
      virtual void prepareBuild(BuildCtx &ctx) { (void)ctx; }

      // Stable type identifier (e.g. "core.wait"), used for (de)serialization.
      virtual std::string typeId() const { return {}; }
      // Whether this is the graph entry node (emitted first during build).
      virtual bool isEntry() const { return false; }
      // Value-pin expression (pull-based); empty for logic-only nodes.
      virtual std::string value(BuildCtx &ctx) { (void)ctx; return {}; }

      // Inline-field literal of each value input (value-pin order).
      virtual std::vector<std::string> valueInputLiterals() { return {}; }
  };
}