/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "ImNodeFlow.h"
#include "json.hpp"
#include "IconsMaterialDesignIcons.h"
#include "../../../utils/string.h"
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
    std::vector<uint64_t> *outUUIDs{nullptr};
    std::vector<uint64_t> *inValUUIDs{nullptr};

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
      source += "    " + varName + " = " + std::to_string(value) + ";\n";
      return *this;
    }

    BuildCtx& incrVar(const std::string &varName, auto value)
    {
      source += "    " + varName + " += " + std::to_string(value) + ";\n";
      return *this;
    }

    BuildCtx& globalVar(const std::string &type, const std::string &name, auto initVal)
    {
      // Deduplicate: only add if a variable with this name isn't already declared
      for(const auto &v : vars) {
        if(v.name == name) return *this;
      }
      vars.push_back(VarDef{type, name, toStr(initVal)});
      return *this;
    }

    std::string globalVar(const std::string &type, auto initVal) {
      std::string varName = "gv_" + std::to_string(vars.size());
      globalVar(type, varName, initVal);
      return varName;
    }

    BuildCtx& jump(uint32_t outIndex) {
      if(outUUIDs && outIndex < outUUIDs->size()) {
        auto uuidOut = (*outUUIDs)[outIndex];
        if(uuidOut) {
          source += "    goto NODE_" + Utils::toHex64(uuidOut) + ";\n";
        } else {
          source += "    return;\n";
        }
      } else {
        source += "    static_assert(false, \"Missing output UUID for jump\");\n";
      }
      return *this;
    }

    BuildCtx& line(const std::string &str) {
      source += "    " + str + "\n";
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
      uint32_t type{};
      std::vector<uint8_t> valInputTypes{};

      virtual void serialize(nlohmann::json &j) = 0;
      virtual void deserialize(nlohmann::json &j) = 0;
      virtual void build(BuildCtx &ctx) = 0;
  };
}