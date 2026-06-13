/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../context.h"
#include "../../../utils/hash.h"
#include "../../../utils/logger.h"

namespace Project::Graph::Node
{
  class ObjEvent : public Base
  {
    private:
      uint16_t eventType{};
      std::string eventValue{};

    public:
      constexpr static const char* NAME = ICON_MDI_EMAIL_FAST_OUTLINE " Send Event";

      ObjEvent()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(90,191,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        // Target object id, fed in as a value. When left unconnected the event is sent
        // to the object running this graph (<Self>). Object ids only exist at runtime,
        // so the target is never picked inside the node editor.
        addIN<TypeValue>("Object", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        valInputTypes.push_back(0);
        valInputTypes.push_back(1);

        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        if(ImTable::start("Node", nullptr, {-1, 70.0f})) {
          ImTable::add("Type", eventType);
          ImTable::add("Value", eventValue);
          ImTable::end();
        }
      }

      void serialize(nlohmann::json &j) override {
        j["eventType"] = eventType;
        j["eventValue"] = eventValue;
      }

      void deserialize(nlohmann::json &j) override {
        eventType = j.value("eventType", 0);
        eventValue = j.value("eventValue", "0");

        // Legacy: the target object used to be a raw runtime id stored in the node.
        // That mechanism is gone; <Self> (0) maps to the new default, any other value
        // can no longer be resolved and must be re-wired via an object value input.
        uint16_t legacyObjId = j.value("objectId", 0);
        if(legacyObjId != 0) {
          Utils::Logger::log(
            "Send Event node referenced object id " + std::to_string(legacyObjId) +
            "; object-id references are no longer supported, defaulting to <Self>",
            Utils::Logger::LEVEL_WARN);
        }
      }

      void build(BuildCtx &ctx) override {

        // check if value is a number or not
        try {
          std::stoul(this->eventValue);
        } catch(...) {
          eventValue = '"' + eventValue + "\"_hash";
        }

        // 0 == <Self>: send to the object running this graph
        if(ctx.inValUUIDs->empty()) {
          ctx.localVar("uint16_t", "t_objId", 0);
        } else {
          ctx.localVar("uint16_t", "t_objId", "res_" + Utils::toHex64(ctx.inValUUIDs->at(0)));
        }

        ctx.localConst("uint16_t", "t_eventType", eventType)
          .localConst("uint32_t", "t_eventVal", eventValue)

          .line("inst->object->getScene().sendEvent(")
          .line("  t_objId == 0 ? inst->object->id : t_objId,")
          .line("  inst->object->id,")
          .line("  t_eventType,")
          .line("  t_eventVal")
          .line(");");
      }
  };
}
