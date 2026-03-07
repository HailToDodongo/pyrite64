/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "layerInspector.h"
#include "../../../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "glm/ext/scalar_common.hpp"
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
  int ctxLayerIndex = -1;

  void drawLayers(std::vector<Project::LayerConf> &layers, const std::string &layerName)
  {
    ImGui::Text("%s", layerName.c_str());
    ImGui::SameLine();
    std::string addLabel = ICON_MDI_PLUS_BOX_OUTLINE " Add##" + layerName;
    ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(addLabel.c_str(), nullptr, true).x) - 8_px);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3_px);
    if (ImGui::Button(addLabel.c_str())) {
      Project::LayerConf layer{};
      layer.name.value = "New Layer";
      layers.push_back(layer);
    }

    int layerIdx = 0;
    for(auto &layer : layers)
    {
      auto tabName = std::to_string(layerIdx) + " - " + layer.name.value + "###" + std::to_string((uint64_t)&layer);

      ImGui::SetCursorPosX(10_px);
      bool open = ImGui::CollapsingHeader(tabName.c_str(), 0);
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(layerName.c_str());
        ctxLayerIndex = layerIdx;
      }

      if (open) {
        ImTable::start("Settings");
        ImTable::addProp("Name", layer.name);
        ImTable::addProp("Z-Compare", layer.depthCompare);
        ImTable::addProp("Z-Write", layer.depthWrite);

        std::vector<ImTable::ComboEntry> blenders{
          {0, "None (Opaque)"},
          {RDPQ_BLENDER_MULTIPLY, "Multiply (Alpha)"},
          {RDPQ_BLENDER_ADDITIVE, "Additive"},
        };
        ImTable::addVecComboBox("Blending", blenders, layer.blender.value);

        ImTable::addProp("Fog", layer.fog);
        if(layer.fog.value)
        {
          std::vector<ImTable::ComboEntry> fogColorModes{
              {1, "Clear-Color"},
              {2, "Custom Color"},
              {3, "Leave Unchanged"},
            };
          ImTable::addVecComboBox("Fog-Mode", fogColorModes, layer.fogColorMode.value);

          if(layer.fogColorMode.value == 2) {
            ImTable::addColor("Fog Color", layer.fogColor.value);
          }

          ImTable::addProp("Fog Min", layer.fogMin);
          ImTable::addProp("Fog Max", layer.fogMax);
        }

        ImTable::end();
        ImGui::Dummy({0, 2});
      }
      ++layerIdx;
    }

    if(ImGui::BeginPopupContextItem(layerName.c_str())) {
      if(ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Duplicate")) {
        auto clone = layers[ctxLayerIndex];
        clone.name.value += " Copy";
        layers.insert(layers.begin() + ctxLayerIndex + 1, clone);
      }
      if(layers.size() > 1 && ImGui::MenuItem(ICON_MDI_TRASH_CAN_OUTLINE " Delete")) {
        layers.erase(layers.begin() + ctxLayerIndex);
      }
      ImGui::EndPopup();
    }
  }
}

Editor::LayerInspector::LayerInspector() {
}

void Editor::LayerInspector::draw() {
  auto scene = ctx.project->getScenes().getLoadedScene();
  if(!scene)return;

  drawLayers(scene->conf.layers3D, "3D Layers");
  ImGui::Dummy({0, 2});

  drawLayers(scene->conf.layersPtx, "Particle Layers");
  ImGui::Dummy({0, 2});

  drawLayers(scene->conf.layers2D, "2D Layers");
  ImGui::Dummy({0, 2});

  std::string resetLabel = "Reset";
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(resetLabel.c_str()).x) * 0.5f - 4);
  if (ImGui::Button(resetLabel.c_str())) {
    scene->resetLayers();
  }

}
