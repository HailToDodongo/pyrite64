/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "textureEditor.h"

#include "../../../../context.h"
#include "../../../imgui/helper.h"

namespace
{
  constexpr bool isPow2(int x) {
    return (x & (x - 1)) == 0 && x > 0;
  }

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
}

void Editor::TextureEditor::draw(Project::Assets::MaterialTex &tex)
{
  auto &assetManager = ctx.project->getAssets();
  const auto &assets = assetManager.getTypeEntries(Project::FileType::IMAGE);

  ImTable::addAssetVecComboBox("Texture", assets, tex.texUUID.value);//, [&data](auto) { data.obj3D.removeMesh(); });

  tex.texSize.value[0] = 32;
  tex.texSize.value[0] = 32;

  auto asset = assetManager.getEntryByUUID(tex.texUUID.value);
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

  ImTable::addProp("Offset", tex.offset);
  tex.offset.value = glm::clamp(tex.offset.value, 0.0f, 1023.75f);

  ImTable::add("Scale");
  tex.scale.value += 5;
  ImGui::SideBySide(
    [&]{ ImGui::Combo("##S0", &tex.scale.value[0], TILE_SCALES); },
    [&]{ ImGui::Combo("##S1", &tex.scale.value[1], TILE_SCALES); }
  );
  tex.scale.value -= 5;

  bool repFix[2] = {!isPow2(tex.texSize.value[0]), !isPow2(tex.texSize.value[1])};
  ImTable::add("Repeat");
  ImGui::BeginGroup();
  ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth() - 4_px);
  for(int i=0; i<2; ++i)
  {
    if(repFix[i])ImGui::BeginDisabled();
    ImGui::InputFloat(i == 0 ? "##R0" : "##R1", &tex.repeat.value[i]);
    if(repFix[i]) {
      ImGui::EndDisabled();
      tex.repeat.value[i] = 1.0f;
    }
    ImGui::PopItemWidth();
    if(i == 0)ImGui::SameLine();
  }
  ImGui::EndGroup();

  tex.repeat.value = glm::clamp(tex.repeat.value, 0.0f, 2048.0f);

  ImTable::add("Mirror");
  ImGui::SideBySide(
    [&]{ ImGui::Checkbox("##MS", &tex.mirrorS.value); },
    [&] {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::CalcItemWidth() - 4_px / 2) - 27_px);
      ImGui::Checkbox("##MT", &tex.mirrorT.value);
    }
  );
}
