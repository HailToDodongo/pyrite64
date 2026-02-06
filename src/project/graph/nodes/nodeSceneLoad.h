/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../context.h"
#include "../../../utils/hash.h"
#include "../../../editor/imgui/helper.h"
#include "../../../editor/imgui/lang.h"

namespace Project::Graph::Node
{
  class SceneLoad : public Base
  {
    private:
      uint32_t sceneId{};

      bool usOwnValue() {
        return getIns()[1].get()->getLinks().empty();
      }

    public:
      constexpr static const char* ICON = ICON_MDI_EARTH_BOX;
      constexpr static const char* NAME = MSG_GRAPH_NODE_SCENELOAD;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      SceneLoad()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(90,191,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);

        valInputTypes.push_back(0);
        valInputTypes.push_back(1);
      }

      void draw() override {
        ImGui::SetNextItemWidth(110.f);

        if (usOwnValue()) {
          ImGui::VectorComboBox("##Scene", ctx.project->getScenes().getEntries(), sceneId);
        }
      }

      void serialize(nlohmann::json &j) override {
        j["sceneId"] = sceneId;
      }

      void deserialize(nlohmann::json &j) override {
        sceneId = j.value("sceneId", 0);
      }

      void build(BuildCtx &ctx) override
      {
        if(ctx.inValUUIDs->empty()) {
          ctx.localConst("uint16_t", "sceneId", std::to_string(sceneId));
        } else {
          auto idStr = Utils::toHex64(ctx.inValUUIDs->at(0));
          ctx.localVar("uint16_t", "sceneId", "res_" + idStr);
        }

        ctx.line("P64::SceneManager::load(sceneId);");
      }
  };
}