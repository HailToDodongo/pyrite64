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
  class SwitchCase : public Base
  {
    private:
      std::vector<uint32_t> cases{};

    public:
      constexpr static const char* ICON = ICON_MDI_CALL_SPLIT;
      constexpr static const char* NAME = MSG_GRAPH_NODE_SWITCHCASE;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      SwitchCase()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xFF, 0x99, 0x55, 0xFF), ImColor(0,0,0,255), 4.0f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        valInputTypes.push_back(0);
        valInputTypes.push_back(1);
      }

      void draw() override {

        uint32_t idx = 0;
        for(auto &c : cases) {
          ImGui::SetNextItemWidth(60.0f);
          ImGui::PushID(idx);
          ImGui::InputScalar("##", ImGuiDataType_U32, &c);
          ImGui::PopID();
          ++idx;
        }

        if(ImGui::Button(Editor::message(MSG_GRAPH_NODE_SWITCHCASE_ADD))) {
          cases.push_back(0);
          addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        }
      }

      void serialize(nlohmann::json &j) override {
        j["cases"] = cases;
      }

      void deserialize(nlohmann::json &j) override {
        if(!j.contains("cases"))return;
        cases = j["cases"].get<std::vector<uint32_t>>();

        for(auto c : cases) {
          addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        }
      }

      void build(BuildCtx &ctx) override
      {
        if(ctx.inValUUIDs->empty()) {
          ctx.localVar("int", "t_comp", 0);
        } else {
          auto idStr = Utils::toHex64(ctx.inValUUIDs->at(0));
          ctx.localVar("int", "t_comp", "res_" + idStr);
        }

        ctx.line("switch(t_comp) {");
        for(size_t i = 0; i < cases.size(); ++i) {
          ctx.line("  case " + std::to_string(cases[i]) + ":")
            .jump(static_cast<uint32_t>(i))
          .line("    break;");
        }
        ctx.line("}");
      }
  };
}