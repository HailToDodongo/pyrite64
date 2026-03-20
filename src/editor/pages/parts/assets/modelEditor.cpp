/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "modelEditor.h"

#include "imgui_internal.h"
#include "../../../../context.h"
#include "../../../imgui/helper.h"

namespace
{
  ImVec2 DEF_WIN_SIZE{400, 400};

  constexpr auto Z_MODES = "None\0Read\0Write\0Read+Write\0";
  constexpr auto AA_MODES = "None\0Standard\0Reduced\0";

  constexpr auto TILE_SCALES =
    "1/32\0"
    "1/16\0"
    "1/8\0"
    "1/4\0"
    "1/2\0"
    "-\0"
    "x2\0"
    "x4\0"
    "x8\0"
    "x16\0"
    "x32\0";

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

  ImVec2 labelWidth = {85_px, -1.0f};

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

      ImTable::start("Mat", nullptr, labelWidth);
      ImTable::addProp("Override", mat.isCustom);
      //if(mat.isCustom.value)
      {
        ImTable::add("Depth");
        ImGui::Combo("##Depth", &mat.zmode.value, Z_MODES);

        ImTable::add("Anti-Alias");
        ImGui::Combo("##AA", &mat.aa.value, AA_MODES);

        ImTable::add("Alpha-Clip");
        ImGui::SliderInt("##AC", &mat.alphaComp.value, 0, 255,
          mat.alphaComp.value == 0 ? "<Off>" : "%d"
        );
      }
      ImTable::end();

      const auto &assets = ctx.project->getAssets().getTypeEntries(Project::FileType::IMAGE);

      auto drawMatTex = [&](Project::Assets::MaterialTex &tex) {
        ImGui::PushID(&tex);

        ImTable::addProp("Dynamic", tex.dynTexture);

        if(tex.dynTexture.value) {
          ImTable::addProp("Size", tex.texSize);
        } else {
          ImTable::addAssetVecComboBox("Texture", assets, tex.texUUID.value);//, [&data](auto) { data.obj3D.removeMesh(); });

          tex.texSize.value[0] = 32;
          tex.texSize.value[0] = 32;

          auto asset = ctx.project->getAssets().getEntryByUUID(tex.texUUID.value);
          if (asset && asset->texture) {
            auto imgSize = asset->texture->getSize();
            tex.texSize.value[0] = imgSize.x;
            tex.texSize.value[1] = imgSize.y;

            // preview image:
            float maxWidth = ImGui::GetContentRegionAvail().x - 8_px;
            if (maxWidth > 128_px)maxWidth = 128_px;
            float imgRatio = imgSize.x / imgSize.y;
            imgSize.x = maxWidth;
            imgSize.y = maxWidth / imgRatio;
            ImGui::Image(ImTextureRef(asset->texture->getGPUTex()), imgSize);
            ImGui::BeginDisabled();
            ImTable::addProp("Size", tex.texSize);
            ImGui::EndDisabled();
          }
        }

        ImTable::addProp("Offset", tex.offset);
        tex.offset.value = glm::clamp(tex.offset.value, 0.0f, 1023.75f);

        ImTable::add("Scale");
        tex.scale.value += 5;
        ImGui::BeginGroup();
        ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth() - 4_px);
          ImGui::Combo("##S0", &tex.scale.value[0], TILE_SCALES);
          ImGui::PopItemWidth(); ImGui::SameLine();
          ImGui::Combo("##S1", &tex.scale.value[1], TILE_SCALES);
          ImGui::PopItemWidth();
        ImGui::EndGroup();
        tex.scale.value -= 5;

        ImTable::addProp("Repeat", tex.repeat);
        tex.repeat.value = glm::clamp(tex.repeat.value, 0.0f, 2048.0f);

        ImTable::add("Mirror");
        ImGui::BeginGroup();
          float width = ImGui::CalcItemWidth() - 4_px;
          ImGui::PushMultiItemsWidths(2, width);
          ImGui::Checkbox("##MS", &tex.mirrorS.value);
          ImGui::PopItemWidth(); ImGui::SameLine();
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width / 2) - 27_px);
          ImGui::Checkbox("##MT", &tex.mirrorT.value);
          ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::PopID();
      };

      if(ImGui::CollapsingSubHeader("Texture 0", ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("TEX1", nullptr, labelWidth))
      {
        drawMatTex(mat.tex0);
        ImTable::end();
      }
      if(ImGui::CollapsingSubHeader("Texture 1", ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("TEX1", nullptr, labelWidth))
      {
        drawMatTex(mat.tex1);
        ImTable::end();
      }

      if(ImGui::CollapsingSubHeader("Sampler", ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("Mat", nullptr, labelWidth))
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
