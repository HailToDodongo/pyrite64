/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../context.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  class ObjEvent : public Base
  {
    private:
      uint16_t objectId{};
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
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        if(ImTable::start("Node", nullptr, {-1, 100.0f})) {

          std::vector<ImTable::ComboEntry> entries;
          entries.push_back({0, "< Self >"});

          auto scene = ctx.project->getScenes().getLoadedScene();
          if(scene)
          {
            for(const auto &[_, obj] : scene->objectsMap) {
              entries.push_back({obj->id, "ID " + std::to_string(obj->id) + " - " + obj->name});
            }
          }

          ImTable::add("Object");
          ImGui::VectorComboBox("##", entries, objectId);
          ImTable::add("Type", eventType);
          ImTable::add("Value", eventValue);
          ImTable::end();
        }
      }

      void serialize(nlohmann::json &j) override {
        j["objectId"] = objectId;
        j["eventType"] = eventType;
        j["eventValue"] = eventValue;
      }

      void deserialize(nlohmann::json &j) override {
        objectId = j.value("objectId", 0);
        eventType = j.value("eventType", 0);
        eventValue = j.value("eventValue", "0");
      }

      void build(BuildCtx &ctx) override {

        // check if value is a number or not
        try {
          std::stoul(this->eventValue);
        } catch(...) {
          eventValue = '"' + eventValue + "\"_hash";
        }

        ctx.localConst("uint16_t", "t_objId", objectId)
          .localConst("uint16_t", "t_eventType", eventType)
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