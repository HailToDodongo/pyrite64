/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  // Provides a reference to a scene object as a value (its runtime object id).
  // The actual object is NOT chosen here, but on the NodeGraph component of the
  // object that uses this graph. Each "Object" node maps to a numbered slot; the
  // component fills those slots, the build resolves them to runtime ids and passes
  // them in via inst->objRefs.
  class ObjRef : public Base
  {
    private:
      uint16_t slot{};
      std::string label{"Object"};

    public:
      constexpr static const char* NAME = ICON_MDI_CUBE_OUTLINE " Object";

      ObjRef()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0x55, 0x99, 0xFF, 0xFF), ImColor(0,0,0,255), 4.0f));

        addOUT<TypeValue>("", PIN_STYLE_VALUE);
      }

      void draw() override {
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("##Label", &label);
        ImGui::SetNextItemWidth(50);
        ImGui::InputScalar("Slot", ImGuiDataType_U16, &slot);
      }

      void serialize(nlohmann::json &j) override {
        // "objRefSlot" doubles as the discriminator the NodeGraph component scans for.
        j["objRefSlot"] = slot;
        j["objRefName"] = label;
      }

      void deserialize(nlohmann::json &j) override {
        slot = j.value("objRefSlot", 0);
        label = j.value("objRefName", std::string{"Object"});
      }

      void build(BuildCtx &ctx) override {
        auto resVar = "res_" + Utils::toHex64(uuid);
        ctx.globalVar("uint16_t", resVar, "inst->objRefs[" + std::to_string(slot) + "]");
      }
  };
}
