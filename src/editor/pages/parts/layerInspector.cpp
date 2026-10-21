/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "layerInspector.h"
#include "../../../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../imgui/notification.h"
#include "../../undoRedo.h"
#include "../../../project/scene/scene.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "glm/ext/scalar_common.hpp"
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
  int ctxLayerIndex = -1;
  std::string pendingName{};

  /**
   * Counts how often models of the scene reference a 3D layer key, including overrides.
   * If 'newKey' is set, those references are changed to it.
   */
  int visitLayerRefs(Project::Scene &scene, uint32_t key, const uint32_t *newKey)
  {
    if(Project::Layer::isIndexRef(key))return 0;

    int count = 0;
    for(auto &[uuid, obj] : scene.objectsMap)
    {
      for(auto &entry : obj->components) {
        if(entry.id != 1 && entry.id != 10)continue; // Model, AnimModel
        const auto &info = Project::Component::TABLE[entry.id];
        auto data = info.funcSerialize(entry);
        if(data.value("layerIdx", 0u) != key)continue;

        ++count;
        if(newKey) {
          data["layerIdx"] = *newKey;
          entry.data = info.funcDeserialize(data);
        }
      }

      // @TODO: overrides are not handled
    }
    return count;
  }

  void drawLayers(Project::Scene &scene, std::vector<Project::LayerConf> &layers, const std::string &layerName, bool isLayer3D)
  {
    ImGui::Text("%s", layerName.c_str());
    ImGui::SameLine();
    std::string addLabel = ICON_MDI_PLUS_BOX_OUTLINE " Add##" + layerName;
    ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(addLabel.c_str(), nullptr, true).x) - 8_px);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3_px);
    if (ImGui::Button(addLabel.c_str())) {
      Project::LayerConf layer{};
      layer.name.value = Project::Layer::makeUniqueName(layers, "New Layer");
      layers.push_back(layer);
      Editor::UndoRedo::getHistory().markChanged("Add Layer");
    }

    // drag & drop to reorder
    std::string dragType = "P64_LAYER_" + layerName;
    int moveFrom = -1;
    int moveTo = -1;

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

      if(ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(dragType.c_str(), &layerIdx, sizeof(layerIdx));
        ImGui::Text("%s", layer.name.value.c_str());
        ImGui::EndDragDropSource();
      }
      if(ImGui::BeginDragDropTarget()) {
        if(auto payload = ImGui::AcceptDragDropPayload(dragType.c_str())) {
          moveFrom = *(const int*)payload->Data;
          moveTo = layerIdx;
        }
        ImGui::EndDragDropTarget();
      }

      if (open) {
        ImTable::start("Settings");
        // applied once editing is done, since the name is what objects reference the layer by
        ImTable::add("Name");
        std::string nameEdit = layer.name.value;
        if(ImGui::InputText("##Name", &nameEdit))pendingName = nameEdit;
        if(ImGui::IsItemDeactivatedAfterEdit() && !pendingName.empty() && pendingName != layer.name.value)
        {
          uint32_t oldKey = Project::Layer::keyFromName(layer.name.value);
          layer.name.value = Project::Layer::makeUniqueName(layers, pendingName, &layer);
          if(isLayer3D) {
            uint32_t newKey = Project::Layer::keyFromName(layer.name.value);
            visitLayerRefs(scene, oldKey, &newKey);
            Editor::Noti::add(Editor::Noti::Type::INFO,
              "Renamed layer, prefabs and other scenes still reference the old name");
          }
          Editor::UndoRedo::getHistory().markChanged("Rename Layer");
        }
        ImTable::addProp("Z-Compare", layer.depthCompare);
        ImTable::addProp("Z-Write", layer.depthWrite);

        std::vector<ImTable::ComboEntry> blenders{
          {0, "None (Opaque)"},
          {RDPQ_BLENDER_MULTIPLY, "Multiply (Alpha)"},
          {RDPQ_BLENDER_ADDITIVE, "Additive"},
        };
        ImTable::addVecComboBox("Blending", blenders, layer.blender.value);

        std::vector<ImTable::ComboEntry> lightModes{
            {0, "Multiply (Default)"},
            {1, "Add (Baked Light)"},
          };
        ImTable::addVecComboBox("Light-Mode", lightModes, layer.lightMode.value);

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

    if(moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo) {
      auto layer = layers[moveFrom];
      layers.erase(layers.begin() + moveFrom);
      layers.insert(layers.begin() + moveTo, layer);
      Editor::UndoRedo::getHistory().markChanged("Move Layer");
    }

    if(ImGui::BeginPopupContextItem(layerName.c_str())) {
      if(ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Duplicate")) {
        auto clone = layers[ctxLayerIndex];
        clone.name.value = Project::Layer::makeUniqueName(layers, clone.name.value + " Copy");
        layers.insert(layers.begin() + ctxLayerIndex + 1, clone);
        Editor::UndoRedo::getHistory().markChanged("Duplicate Layer");
      }
      if(layers.size() > 1 && ImGui::MenuItem(ICON_MDI_TRASH_CAN_OUTLINE " Delete")) {
        int refCount = isLayer3D ? visitLayerRefs(scene, Project::Layer::keyFromName(layers[ctxLayerIndex].name.value), nullptr) : 0;
        layers.erase(layers.begin() + ctxLayerIndex);
        if(refCount > 0) {
          Editor::Noti::add(Editor::Noti::Type::INFO, std::to_string(refCount)
            + " reference(s) in this scene now point to a missing layer and use the default one");
        }
        Editor::UndoRedo::getHistory().markChanged("Delete Layer");
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

  drawLayers(*scene, scene->conf.layers3D, "3D Layers", true);
  ImGui::Dummy({0, 2});

  drawLayers(*scene, scene->conf.layersPtx, "Particle Layers", false);
  ImGui::Dummy({0, 2});

  drawLayers(*scene, scene->conf.layers2D, "2D Layers", false);
  ImGui::Dummy({0, 2});

  std::string resetLabel = "Reset";
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(resetLabel.c_str()).x) * 0.5f - 4);
  if (ImGui::Button(resetLabel.c_str())) {
    scene->resetLayers();
  }

}
