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
  class Compare : public Base
  {
    private:
      int compType{};

      constexpr static std::array<const char*, 6> COMP_TYPES = {
        ICON_MDI_EQUAL                 ,//" Equal",
        ICON_MDI_NOT_EQUAL             ,//" Not Equal",
        ICON_MDI_LESS_THAN             ,//" Less Than",
        ICON_MDI_LESS_THAN_OR_EQUAL    ,//" Less Than/Equal",
        ICON_MDI_GREATER_THAN          ,//" Greater Than",
        ICON_MDI_GREATER_THAN_OR_EQUAL ,//" Greater Than/Equal",
      };

      void updateTitle() {
        setTitle(std::string(COMP_TYPES[compType]) + Editor::message(NAME));
      }

    public:
      constexpr static const char* ICON = ICON_MDI_LESS_THAN_OR_EQUAL;
      constexpr static const char* NAME = MSG_GRAPH_NODE_COMPARE;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Compare()
      {
        uuid = Utils::Hash::randomU64();
        updateTitle();
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xFF, 0x99, 0x55, 0xFF), ImColor(0,0,0,255), 4.0f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);

        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_COMPARE_TRUE), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_COMPARE_FALSE), PIN_STYLE_LOGIC);
      }

      void draw() override {
        if(ImTable::start("Node", nullptr, 80.0f)) {
          if(ImTable::addComboBox(Editor::message(MSG_GRAPH_NODE_COMPARE_OPER), compType, COMP_TYPES.data(), COMP_TYPES.size())) {
            updateTitle();
          }
          ImTable::end();
        }

      }

      void serialize(nlohmann::json &j) override {
      }

      void deserialize(nlohmann::json &j) override {
      }

      void build(BuildCtx &ctx) override {
      }
  };
}