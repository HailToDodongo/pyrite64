/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "modelEditor.h"
#include "../../../../context.h"
#include "../../../imgui/helper.h"

namespace
{
  ImVec2 DEF_WIN_SIZE{400, 400};

  constexpr auto Z_MODES = "None\0Read\0Write\0Read+Write\0";
  constexpr auto AA_MODES = "None\0Standard\0Reduced\0";

  constexpr auto DITHER_MODES = "Square / Square\0"
    "Square / Inv. Square\0"
    "Square / Noise\0"
    "Square / None\0"
    "Bayer / Bayer\0"
    "Bayer / Inv. Bayer\0"
    "Bayer / Noise\0"
    "Bayer / None\0"
    "Noise / Square\0"
    "Noise / Inv. Square\0"
    "Noise / Noise\0"
    "Noise / None\0"
    "None / Bayer\0"
    "None / Inv. Bayer\0"
    "None / Noise\0"
    "None / None\0";

}

bool Editor::ModelEditor::draw(ImGuiID defDockId)
{
  auto model = ctx.project->getAssets().getEntryByUUID(assetUUID);
  if(!model)return false;

  winName = "Model: " + model->name;
  ImGui::SetNextWindowSize(DEF_WIN_SIZE, ImGuiCond_FirstUseEver);
  auto screenSize = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowPos({(screenSize.x - DEF_WIN_SIZE.x) / 2, (screenSize.y - DEF_WIN_SIZE.y) / 2}, ImGuiCond_FirstUseEver);

  bool isOpen = true;
  ImGui::Begin(winName.c_str(), &isOpen);
  ImGui::Text("Model: %s", model->name.c_str());

  for(auto &entry : model->model.materials)
  {
    auto label = "Material: " + entry.first;
    ImGui::PushID(label.c_str());
    if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      auto &mat = entry.second;

      ImTable::start("Mat");
      ImTable::addProp("Override", mat.isCustom);
      //if(mat.isCustom.value)
      {
        ImTable::add("Depth");
        ImGui::Combo("##Depth", &mat.zmode.value, Z_MODES);

        ImTable::add("Anti-Aliasing");
        ImGui::Combo("##AA", &mat.aa.value, AA_MODES);

        ImTable::add("Alpha-Clip");
        ImGui::SliderInt("##AC", &mat.alphaComp.value, 0, 255,
          mat.alphaComp.value == 0 ? "<Off>" : "%d"
        );
      }
      ImTable::end();

      if(ImGui::CollapsingSubHeader("Sampler", ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("Mat"))
      {
        ImTable::addProp("Perspective", mat.persp);

        ImTable::add("Dither");
        ImGui::Combo("##Dither", &mat.dither.value, DITHER_MODES);

        ImTable::add("Filter");
        int val = mat.filter.value == 0 ? 0 : 1; // map 2->1
        ImGui::Combo("##Filtering", &val, "Nearest\0Bilinear\0");
        mat.filter.value = val == 0 ? 0 : 2;


        ImTable::end();
      }

    }
    ImGui::PopID();

  }

  ImGui::End();
  return isOpen;
}

void Editor::ModelEditor::focus() const
{
  ImGui::SetWindowFocus(winName.c_str());
}
