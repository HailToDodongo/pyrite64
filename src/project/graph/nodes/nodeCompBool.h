/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  class CompBool : public Base
  {
    private:

    public:
      constexpr static const char* NAME = ICON_MDI_CALL_SPLIT " If-Else";

      CompBool()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0xFF, 0x99, 0x55, 0xFF), ImColor(0,0,0,255), 4.0f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        valInputTypes.push_back(0);
        valInputTypes.push_back(1);

        (void)addOUT<TypeLogic>("True", PIN_STYLE_LOGIC);
        (void)addOUT<TypeLogic>("False", PIN_STYLE_LOGIC);
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