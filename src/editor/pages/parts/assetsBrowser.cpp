/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetsBrowser.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"
#include "../../../context.h"

using FileType = Project::FileType;

namespace
{
  constexpr int TAB_IDX_SCENES = 0;
  constexpr int TAB_IDX_ASSETS = 1;
  constexpr int TAB_IDX_SCRIPTS = 2;
  constexpr int TAB_IDX_PREFABS = 3;

  struct TabDef
  {
    const char* name;
    std::vector<FileType> fileTypes{};
    bool showScenes{false};
  };

  const char* getterScene(void* user_data, int idx)
  {
    auto &scenes = ctx.project->getScenes().getEntries();
    if (idx < 0 || idx >= scenes.size())return "<Select Scene>";
    return scenes[idx].name.c_str();
  }

}

void Editor::AssetsBrowser::draw() {
  auto &scenes = ctx.project->getScenes().getEntries();

  const std::array<TabDef, 4> TABS{
    TabDef{
      .name = ICON_MDI_EARTH_BOX "  Scenes",
      .showScenes = true
    },
    TabDef{
      .name = ICON_MDI_FILE "  Assets",
      .fileTypes = {FileType::IMAGE, FileType::AUDIO, FileType::MODEL_3D, FileType::FONT}
    },
    TabDef{
      .name = ICON_MDI_SCRIPT_OUTLINE "  Scripts",
      .fileTypes = {FileType::CODE_OBJ, FileType::CODE_GLOBAL, FileType::NODE_GRAPH}
    },
    TabDef{
      .name = ICON_MDI_PACKAGE_VARIANT_CLOSED "  Prefabs",
      .fileTypes = {FileType::PREFAB}
    },
  };

  ImGui::BeginChild("LEFT", ImVec2(94, 0), ImGuiChildFlags_Borders);
  for (int i=0; i<TABS.size(); ++i) {
    bool isActive = i == activeTab;
    if (ImGui::Selectable(TABS[i].name, isActive))activeTab = i;
  }
  ImGui::EndChild();

  float sceneOptionsWidth = 140;

  if(activeTab == TAB_IDX_SCENES)
  {
    // right align
    ImGui::SameLine();
    ImGui::BeginChild("END", ImVec2(sceneOptionsWidth, 0), ImGuiChildFlags_Borders);

    ImGui::Text(message(Message::ON_BOOT));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::VectorComboBox("##Boot", scenes, ctx.project->conf.sceneIdOnBoot);

    ImGui::Text(message(Message::ON_RESET));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::VectorComboBox("##Reset", scenes, ctx.project->conf.sceneIdOnReset);

    ImGui::EndChild();
  }

  const auto &tab = TABS[activeTab];

  auto availWidth = ImGui::GetContentRegionAvail().x - 4;
  if(activeTab == TAB_IDX_SCENES)availWidth -= sceneOptionsWidth;

  ImGui::SameLine();
  ImGui::BeginChild("RIGHT");

  float imageSize = 64;
  float itemWidth = imageSize + 18;
  float currentWidth = 0.0f;
  ImVec2 textBtnSize{imageSize+12, imageSize+8};

  float cursorStartX = ImGui::GetCursorPosX();
  float cursorY = ImGui::GetCursorPosY();

  auto checkLineBreak = [&]() {
    if ((currentWidth+itemWidth*2) > availWidth) {
      currentWidth = 0.0f;
      cursorY += imageSize + 28;
      ImGui::SetCursorPos({cursorStartX, cursorY});
    } else {
      if (currentWidth != 0)ImGui::SameLine();
    }
    currentWidth += itemWidth;
  };

  for(const auto type : tab.fileTypes)
  {
    for (const auto &asset : ctx.project->getAssets().getTypeEntries(type))
    {
      checkLineBreak();

      auto icon = ImTextureRef(nullptr);
      const char* iconTxt = ICON_MDI_FILE_OUTLINE;
      if (asset.texture) {
        icon = ImTextureRef(asset.texture->getGPUTex());
      } else {
        if (asset.type == FileType::MODEL_3D) {
          iconTxt = ICON_MDI_CUBE_OUTLINE;
        } else if (asset.type == FileType::AUDIO) {
          iconTxt = ICON_MDI_MUSIC;
        } else if (asset.type == FileType::FONT) {
          iconTxt = ICON_MDI_FORMAT_FONT;
        } else if (asset.type == FileType::PREFAB) {
          iconTxt = ICON_MDI_PACKAGE_VARIANT_CLOSED;
        } else if (asset.type == FileType::CODE_OBJ || asset.type == FileType::CODE_GLOBAL) {
          iconTxt = ICON_MDI_LANGUAGE_CPP;
        } else if (asset.type == FileType::NODE_GRAPH) {
          iconTxt = ICON_MDI_GRAPH_OUTLINE;
        }
      }

      bool isSelected = (ctx.selAssetUUID == asset.getUUID());
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      bool clicked{false};

      auto sPos = ImGui::GetCursorScreenPos();
      {
        auto size = ImGui::CalcTextSize(asset.name.c_str());
        ImVec2 rextMin{sPos.x,                sPos.y + imageSize + 8};
        ImVec2 rextMax{sPos.x + imageSize+14, sPos.y + imageSize + 8 + 16};

        auto drawText = asset.name;
        if((size.x+3) > (rextMax.x - rextMin.x))
        {
          ImGui::RenderTextEllipsis(
            ImGui::GetWindowDrawList(), rextMin, rextMax, 0,
            asset.name.c_str(), asset.name.c_str() + asset.name.size(),
            nullptr
          );
        } else {
          ImGui::GetWindowDrawList()->AddText(
            {rextMin.x + ((rextMax.x - rextMin.x) - size.x) * 0.5f,
             rextMin.y + ((rextMax.y - rextMin.y) - size.y) * 0.5f},
            ImGui::GetColorU32(ImGuiCol_Text),
            asset.name.c_str()
          );
        }

        /*ImGui::RenderTextEllipsis(
          ImGui::GetWindowDrawList(),
          {sPos.x,                sPos.y + imageSize + 6},
          {sPos.x + imageSize+18, sPos.y + imageSize + 6 + 16},
          0,
          asset.name.c_str(), asset.name.c_str() + asset.name.size(),
          nullptr
        );*/
      }

      if(icon._TexID)
      {
        clicked = ImGui::ImageButton(asset.name.c_str(), icon,
          {imageSize, imageSize}, {0,0}, {1,1}, {0,0,0,0},
          {1,1,1, asset.conf.exclude ? 0.25f : 1.0f}
        );

      } else {
        ImGui::PushFont(nullptr, 40.0f);
        ImGui::PushID((int)asset.getUUID());
          clicked = ImGui::Button(iconTxt, textBtnSize);
        ImGui::PopID();
        ImGui::PopFont();
      }

      if (clicked) {
        ctx.selAssetUUID = asset.getUUID() == ctx.selAssetUUID ? 0 : asset.getUUID();
      }

      if (ImGui::BeginDragDropSource()) {
        if(icon._TexID) {
          ImGui::ImageButton(asset.name.c_str(), icon, {imageSize*0.75f, imageSize*0.75f});
        } else {
          ImGui::Button(iconTxt, textBtnSize);
        }
        ImGui::SetDragDropPayload("ASSET", &asset.conf.uuid, sizeof(asset.conf.uuid));
        ImGui::EndDragDropSource();
      }

      if(isSelected)ImGui::PopStyleColor(2);

      if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::SetTooltip("File: %s", asset.name.c_str());
      }
    }
  }

  if(tab.showScenes)
  {
    for (const auto &scene : scenes)
    {
      checkLineBreak();
      auto activeScene = ctx.project->getScenes().getLoadedScene();

      bool isSelected = activeScene && (activeScene->getId() == scene.id);
      if(isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.5f,0.7f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.5f,0.5f,0.7f,0.8f});
      }

      if (ImGui::Button(scene.name.c_str(), textBtnSize)) {
        ctx.project->getScenes().loadScene(scene.id);
      }

      if(isSelected)ImGui::PopStyleColor(2);

      if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::SetTooltip("Scene: %s\nID: %d", scene.name.c_str(), scene.id);
      }
    }
  }

  if (activeTab == TAB_IDX_SCRIPTS || activeTab == TAB_IDX_SCENES)
  {
    checkLineBreak();
    // set textsize to larger

    ImGui::PushFont(nullptr, 32.0f);
    if (ImGui::Button(
      (activeTab == TAB_IDX_SCRIPTS) ? ICON_MDI_FILE_DOCUMENT_PLUS_OUTLINE : ICON_MDI_EARTH_BOX_PLUS,
      textBtnSize
    )) {
      if(activeTab == TAB_IDX_SCRIPTS) {
        ImGui::OpenPopup("NewScript");
      } else {
        ctx.project->getScenes().add();
      }
    }
    ImGui::PopFont();
  }

  ImGui::Dummy({0, 10});

  if(ImGui::BeginPopup("NewScript"))
  {
    static char scriptName[128] = "New_Script";
    ImGui::Text("Enter script name:");
    ImGui::InputText("##Name", scriptName, sizeof(scriptName));
    if (ImGui::Button("Create")) {
      ctx.project->getAssets().createScript(scriptName);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }


  ImGui::EndChild();
}
