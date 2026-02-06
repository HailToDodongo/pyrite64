/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../editor/imgui/helper.h"
#include "../../../editor/imgui/lang.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  class Value : public Base
  {
    private:
      uint16_t value{};



    public:
      constexpr static const char* ICON = ICON_MDI_NUMERIC;
      constexpr static const char* NAME = MSG_GRAPH_NODE_VALUE;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Value()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xFF, 0x99, 0x55, 0xFF), ImColor(0,0,0,255), 4.0f));

        addOUT<TypeValue>("", PIN_STYLE_VALUE);
      }

      void draw() override {
        ImGui::SetNextItemWidth(50);
        ImGui::InputScalar("##Value", ImGuiDataType_U16, &value);
      }

      void serialize(nlohmann::json &j) override {
        j["value"] = value;
      }

      void deserialize(nlohmann::json &j) override {
        value = j.value("value", 0);
      }

      void build(BuildCtx &ctx) override {
        auto resVar = "res_" + Utils::toHex64(uuid);
        ctx.globalVar("int", resVar, value);
      }
  };
}