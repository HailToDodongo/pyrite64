/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "layerInspector.h"
#include "../../../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"

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
    std::string addLabel = std::string{ICON_MDI_PLUS_BOX_OUTLINE} + Editor::message(MSG_LAYERS_ADD);
    std::string addLabelId = addLabel + "##" + layerName;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(addLabel.c_str()).x) - 17);
    if (ImGui::Button(addLabelId.c_str())) {
      Project::LayerConf layer{};
      layer.name.value = Editor::message(MSG_LAYERS_NEW_LAYER);
      layers.push_back(layer);
    }

    int layerIdx = 0;
    for(auto &layer : layers)
    {
      auto tabName = std::to_string(layerIdx) + " - " + layer.name.value + "###" + std::to_string((uint64_t)&layer);

      ImGui::SetCursorPosX(10);
      bool open = ImGui::CollapsingHeader(tabName.c_str(), 0);
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(layerName.c_str());
        ctxLayerIndex = layerIdx;
      }

      if (open) {
        ImTable::start("Settings");
        ImTable::addProp(Editor::message(MSG_LAYERS_NAME), layer.name);
        ImTable::addProp(Editor::message(MSG_LAYERS_ZCOMPARE), layer.depthCompare);
        ImTable::addProp(Editor::message(MSG_LAYERS_ZWRITE), layer.depthWrite);

        std::vector<ImTable::ComboEntry> blenders{
          {0, Editor::message(MSG_LAYERS_BLENDERS_NONE)},
          {RDPQ_BLENDER_MULTIPLY, Editor::message(MSG_LAYERS_BLENDERS_MULTIPLY)},
          {RDPQ_BLENDER_ADDITIVE, Editor::message(MSG_LAYERS_BLENDERS_ADDITIVE)},
        };
        ImTable::addVecComboBox(Editor::message(MSG_LAYERS_BLENDING), blenders, layer.blender.value);

        ImTable::addProp(Editor::message(MSG_LAYERS_FOG), layer.fog);
        if(layer.fog.value)
        {
          std::vector<ImTable::ComboEntry> fogColorModes{
              {1, Editor::message(MSG_LAYERS_FOG_MODES_CLEAR)},
              {2, Editor::message(MSG_LAYERS_FOG_MODES_CUSTOM)},
              {3, Editor::message(MSG_LAYERS_FOG_MODES_UNCHANGED)},
            };
          ImTable::addVecComboBox(Editor::message(MSG_LAYERS_FOG_MODE), fogColorModes, layer.fogColorMode.value);

          if(layer.fogColorMode.value == 2) {
            ImTable::addColor(Editor::message(MSG_LAYERS_FOG_COLOR), layer.fogColor.value);
          }

          ImTable::addProp(Editor::message(MSG_LAYERS_FOG_MIN), layer.fogMin);
          ImTable::addProp(Editor::message(MSG_LAYERS_FOG_MAX), layer.fogMax);
        }

        ImTable::end();
        ImGui::Dummy({0, 2});
      }
      ++layerIdx;
    }

    if(ImGui::BeginPopupContextItem(layerName.c_str())) {
      if(ImGui::MenuItem((std::string{ICON_MDI_CONTENT_COPY} + Editor::message(MSG_LAYERS_DUPLICATE)).c_str())) {
        auto clone = layers[ctxLayerIndex];
        clone.name.value += Editor::message(MSG_LAYERS_COPY);
        layers.insert(layers.begin() + ctxLayerIndex + 1, clone);
      }
      if(layers.size() > 1 && ImGui::MenuItem((std::string{ICON_MDI_TRASH_CAN_OUTLINE} + Editor::message(MSG_LAYERS_DELETE)).c_str())) {
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

  drawLayers(scene->conf.layers3D, Editor::message(MSG_LAYERS_3D));
  ImGui::Dummy({0, 2});

  drawLayers(scene->conf.layersPtx, Editor::message(MSG_LAYERS_PARTICLE));
  ImGui::Dummy({0, 2});

  drawLayers(scene->conf.layers2D, Editor::message(MSG_LAYERS_2D));
  ImGui::Dummy({0, 2});

  std::string resetLabel = Editor::message(MSG_LAYERS_RESET);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(resetLabel.c_str()).x) * 0.5f - 4);
  if (ImGui::Button(resetLabel.c_str())) {
    scene->resetLayers();
  }

}
