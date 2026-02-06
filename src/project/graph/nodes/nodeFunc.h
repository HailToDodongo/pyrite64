/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"
#include "../../../editor/imgui/lang.h"

namespace Project::Graph::Node
{
  class Func : public Base
  {
    private:
      std::string funcName{};
      std::string arg0{"0"};

    public:
      constexpr static const char* ICON = ICON_MDI_FUNCTION;
      constexpr static const char* NAME = MSG_GRAPH_NODE_FUNCTION;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      void updateTitle() {
        if(funcName.empty()) {
          setTitle(getname());
        } else {
          setTitle(ICON_MDI_FUNCTION " " + funcName);// + "(" + arg0 + ")");
        }
      }

      Func()
      {
        uuid = Utils::Hash::randomU64();
        updateTitle();
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(90,191,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        addOUT<TypeValue>("", PIN_STYLE_VALUE);
      }

      void draw() override {
        //ImGui::Text("Func:");
        //ImGui::SameLine();
        bool changed = false;
        auto textWidth  = ImGui::CalcTextSize(funcName.c_str()).x + 16;
        ImGui::SetNextItemWidth(fmaxf(textWidth, 50.0f));
        changed |= ImGui::InputText("##FuncName", &funcName);
        ImGui::SetNextItemWidth(fmaxf(textWidth, 50.0f));
        changed |= ImGui::InputText("##Arg", &arg0);

        if(changed) {
          updateTitle();
        }

        //showIN("", 0, ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
      }

      void serialize(nlohmann::json &j) override {
        j["funcName"] = funcName;
        j["arg0"] = arg0;
      }

      void deserialize(nlohmann::json &j) override {
        funcName = j.value("funcName", "");
        arg0 = j.value("arg0", "0");
        updateTitle();
      }

      void build(BuildCtx &ctx) override {
        auto uuidStr = std::to_string(Utils::Hash::crc32(funcName));
        auto funcVar = ctx.globalVar("UserFunc", "P64::NodeGraph::getFunction("+uuidStr+")");

        std::string arg = this->arg0;
        try {
          std::stoul(arg);
        } catch(...) {
          arg = '"' + arg + "\"_hash";
        }

        auto resVar = "res_" + Utils::toHex64(uuid);
        ctx.globalVar("int", resVar, 0)
          .localConst("uint32_t", "t_arg", arg)
          .line(resVar + " = " + funcVar + "(t_arg);");

      }
  };
}