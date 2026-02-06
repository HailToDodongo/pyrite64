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
  class Start : public Base
  {
    private:


    public:
      constexpr static const char* ICON = ICON_MDI_PLAY;
      constexpr static const char* NAME = MSG_GRAPH_NODE_START;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Start()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xEE, 0xEE, 0xEE, 0xFF), ImColor(0,0,0,255), 4.0f));

        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        //addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        //addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::Text(Editor::message(MSG_GRAPH_NODE_START_ONSTART));
        //ImGui::Text("On Event");
        //ImGui::Text("On Collision");
      }

      void serialize(nlohmann::json &j) override {
      }

      void deserialize(nlohmann::json &j) override {
      }

      void build(BuildCtx &ctx) override {
      }
  };
}