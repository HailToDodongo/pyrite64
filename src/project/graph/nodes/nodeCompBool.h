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
  class CompBool : public Base
  {
    private:

    public:
      constexpr static const char* ICON = ICON_MDI_CALL_SPLIT;
      constexpr static const char* NAME = MSG_GRAPH_NODE_COMPBOOL;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      CompBool()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xFF, 0x99, 0x55, 0xFF), ImColor(0,0,0,255), 4.0f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        valInputTypes.push_back(0);
        valInputTypes.push_back(1);

        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_COMPBOOL_TRUE), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>(Editor::message(MSG_GRAPH_NODE_COMPBOOL_FALSE), PIN_STYLE_LOGIC);
      }

      void draw() override {

      }

      void serialize(nlohmann::json &j) override {
      }

      void deserialize(nlohmann::json &j) override {
      }

      void build(BuildCtx &ctx) override
      {
        if(ctx.inValUUIDs->empty()) {
          ctx.localVar("int", "t_comp", 0);
        } else {
          auto idStr = Utils::toHex64(ctx.inValUUIDs->at(0));
          ctx.localVar("int", "t_comp", "res_" + idStr);
        }

        ctx.line("if(t_comp) {")
          .jump(0)
        .line("} else {")
          .jump(1)
        .line("}");
      }
  };
}