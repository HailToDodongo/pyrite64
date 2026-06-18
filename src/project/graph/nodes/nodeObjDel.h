/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/hash.h"
#include "../../../utils/logger.h"

namespace Project::Graph::Node
{
  class ObjDel : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_TRASH_CAN_OUTLINE " Delete Object";

      ObjDel()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(191,90,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        // Target object id, fed in as a value. Unconnected deletes the object running
        // this graph (<Self>). Object ids only exist at runtime, so the target is never
        // picked inside the node editor.
        addIN<TypeValue>("Object", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        valInputTypes.push_back(0);
        valInputTypes.push_back(1);

        (void)addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
      }

      void serialize(nlohmann::json &j) override {
      }

      void deserialize(nlohmann::json &j) override {
        // Legacy: the target object used to be a raw runtime id stored in the node.
        // Only <Self> (0) was ever selectable, so anything else is dropped to <Self>.
        uint16_t legacyObjId = j.value("objectId", 0);
        if(legacyObjId != 0) {
          Utils::Logger::log(
            "Delete Object node referenced object id " + std::to_string(legacyObjId) +
            "; object-id references are no longer supported, defaulting to <Self>",
            Utils::Logger::LEVEL_WARN);
        }
      }

      void build(BuildCtx &ctx) override {
        if(ctx.inValUUIDs->empty()) {
          ctx.line("inst->object->remove();"); // <Self>
        } else {
          ctx.localVar("uint16_t", "t_objId", "res_" + Utils::toHex64(ctx.inValUUIDs->at(0)));
          ctx.line("auto* t_obj = t_objId == 0 ? inst->object : inst->object->getScene().getObjectById(t_objId);");
          ctx.line("if(t_obj) t_obj->remove();");
        }
      }
  };
}
