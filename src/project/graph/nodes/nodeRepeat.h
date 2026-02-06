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
  class Repeat : public Base
  {
    private:
      uint32_t count{};

    public:
      constexpr static const char* ICON = ICON_MDI_REPEAT;
      constexpr static const char* NAME = MSG_GRAPH_NODE_REPEAT;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Repeat()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(90,191,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_REPEAT_LOOP), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_REPEAT_EXIT), PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(50.f);
        ImGui::InputScalar("##Count", ImGuiDataType_U32, &count);
        //showIN("", 0, ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
      }

      void serialize(nlohmann::json &j) override {
        j["count"] = count;
      }

      void deserialize(nlohmann::json &j) override {
        count = j.value("count", 0);
      }

      void build(BuildCtx &ctx) override
      {
        auto stdCount = std::to_string(count);
        auto varCounter = ctx.globalVar("uint8_t", 0);

        ctx.incrVar(varCounter, 1)
        .line("if("+varCounter+" == "+stdCount+") {")
          .setVar(varCounter, 0)
          .jump(1)
        .line("}")
        ;
      }
  };
}