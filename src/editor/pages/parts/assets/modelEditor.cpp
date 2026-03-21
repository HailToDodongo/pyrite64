/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "modelEditor.h"

#include "libdragon.h"
#include "ccMapping.h"
#include "imgui_internal.h"
#include "../../../../context.h"
#include "../../../imgui/helper.h"


namespace
{
  ImVec2 DEF_WIN_SIZE{400, 400};

  constexpr bool isPow2(int x) {
    return (x & (x - 1)) == 0 && x > 0;
  }

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

  constexpr auto VERTEX_EFFECTS =
    "None\0"
    "Spherical UV\0"
    "Cel-shade Color\0"
    "Cel-shade Alpha\0"
    "Outline\0"
    "UV Offset\0";

  void toggleProp(const char* name, bool &propState, auto cb)
  {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();

    ImGui::PushFont(nullptr, 18.0_px);

    if(ImGui::IconButton(
      propState
      ? ICON_MDI_CHECKBOX_MARKED_CIRCLE
      : ICON_MDI_CHECKBOX_BLANK_CIRCLE_OUTLINE,
      {24_px,24_px},
      ImVec4{1,1,1,1}
    )) {
      propState = !propState;
      //Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    }
    ImGui::PopFont();
    ImGui::SameLine();

    ImGui::SameLine();
    ImGui::Text("%s", name);
    ImGui::TableSetColumnIndex(1);

    if(!propState)ImGui::BeginDisabled();
    ImGui::PushID(name);
    cb();
    ImGui::PopID();
    if(!propState)ImGui::EndDisabled();
  }

  template<typename T>
  void toggleProp(const char* name, bool &propState, Property<T> &prop)
  {
    toggleProp(name, propState, [&prop](){
      ImTable::typedInput(&prop.value);
    });
  }

  void sideBySide(auto cbA, auto cbB)
  {
    ImGui::BeginGroup();
    ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth() - 4_px);
    cbA();
    ImGui::PopItemWidth(); ImGui::SameLine();
    cbB();
    ImGui::PopItemWidth();
    ImGui::EndGroup();
  }

  void printCC(const char* a, const char* b, const char* c, const char* d)
  {
    auto nonZero = [](const char* s){ return s[0] != '0'; };

    std::string s{};
    // check if mul does something
    if(nonZero(c) && (nonZero(a) || nonZero(b)))
    {
      if(nonZero(a) && nonZero(b)) {
        s += std::string{"("} + a + " - " + b + ")";
      } else {
        s += nonZero(a) ? a : b;
      }
      s += std::string{" * "} + c;
    }

    if(nonZero(d)) {
      if(!s.empty())s += " + ";
      s += d;
    }
    if(s.empty())s = "0";

    ImGui::Text("%s", s.c_str());
  }
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

  ImVec2 labelWidth = {85_px, -1.0f};

  auto subSection = [&labelWidth](const char* name, auto cb)
  {
    if(ImGui::CollapsingSubHeader(name, ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start(name, nullptr, labelWidth))
    {
      cb();
      ImTable::end();
    }
  };

  for(auto &entry : model->model.materials)
  {
    auto label = "Material: " + entry.first;
    ImGui::PushID(label.c_str());
    if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      auto &mat = entry.second;

      ImTable::start("General", nullptr, labelWidth);
      ImTable::addProp("Override", mat.isCustom);

      ImTable::end();

      subSection("Color-Combiner", [&]
      {
        bool twoCycle = mat.cc.value & RDPQ_COMBINER_2PASS;
        ImTable::add("2-Cycle");
        ImGui::Checkbox("##2C", &twoCycle);

        glm::ivec4 cc[2], cca[2];
        N64::CC::unpackCC(mat.cc.value, cc[0], cca[0], cc[1], cca[1]);
        for(int c = 0; c < (twoCycle ? 2 : 1); ++c)
        {
          ImGui::PushID(c);
          ImTable::add("A");
          sideBySide(
            [&]{ ImGui::Combo("##C0C_A",  &cc[c][0], N64::CC::NAMES_COL_A.data(), N64::CC::NAMES_COL_A.size()); },
            [&]{ ImGui::Combo("##C0A_A", &cca[c][0], N64::CC::NAMES_ALPHA_A.data(), N64::CC::NAMES_ALPHA_A.size()); }
          );
          ImTable::add("B");
          sideBySide(
            [&]{ ImGui::Combo("##C0C_B",  &cc[c][1], N64::CC::NAMES_COL_B.data(), N64::CC::NAMES_COL_B.size()); },
            [&]{ ImGui::Combo("##C0A_B", &cca[c][1], N64::CC::NAMES_ALPHA_B.data(), N64::CC::NAMES_ALPHA_B.size()); }
          );
          ImTable::add("C");
          sideBySide(
            [&]{ ImGui::Combo("##C0C_C",  &cc[c][2], N64::CC::NAMES_COL_C.data(), N64::CC::NAMES_COL_C.size()); },
            [&]{ ImGui::Combo("##C0A_C", &cca[c][2], N64::CC::NAMES_ALPHA_C.data(), N64::CC::NAMES_ALPHA_C.size()); }
          );
          ImTable::add("D");
          sideBySide(
            [&]{ ImGui::Combo("##C0C_D",  &cc[c][3], N64::CC::NAMES_COL_D.data(), N64::CC::NAMES_COL_D.size()); },
            [&]{ ImGui::Combo("##C0A_D", &cca[c][3], N64::CC::NAMES_ALPHA_D.data(), N64::CC::NAMES_ALPHA_D.size()); }
          );
          ImGui::PopID();

          ImTable::add("Color");
          printCC(
            N64::CC::NAMES_COL_A[cc[c][0]], N64::CC::NAMES_COL_B[cc[c][1]],
            N64::CC::NAMES_COL_C[cc[c][2]], N64::CC::NAMES_COL_D[cc[c][3]]
          );
          ImTable::add("Alpha");
          printCC(
            N64::CC::NAMES_ALPHA_A[cca[c][0]], N64::CC::NAMES_ALPHA_B[cca[c][1]],
            N64::CC::NAMES_ALPHA_C[cca[c][2]], N64::CC::NAMES_ALPHA_D[cca[c][3]]
         );

          if(twoCycle && c == 0) {
            ImGui::Dummy({0, 4_px});
          }
        }

        mat.cc.value = N64::CC::packCC(cc[0], cca[0], cc[1], cca[1]);
        if(twoCycle)mat.cc.value |= RDPQ_COMBINER_2PASS;
      });

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
        sideBySide(
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
        sideBySide(
          [&]{ ImGui::Checkbox("##MS", &tex.mirrorS.value); },
          [&] {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::CalcItemWidth() - 4_px / 2) - 27_px);
            ImGui::Checkbox("##MT", &tex.mirrorT.value);
          }
        );
        ImGui::PopID();
      };

      subSection("Texture 0", [&]{ drawMatTex(mat.tex0); });
      subSection("Texture 1", [&]{ drawMatTex(mat.tex1); });
      subSection("Sampling", [&]
      {
        toggleProp("Perspect.", mat.perspSet.value, mat.persp);

        toggleProp("Dither", mat.ditherSet.value, [&] {
          ImGui::Combo("##Dither", &mat.dither.value, DITHER_MODES);
        });

        toggleProp("Filtering", mat.filterSet.value, [&] {
          int val = mat.filter.value == 0 ? 0 : 1; // map 2->1
          ImGui::Combo("##", &val, "Nearest\0Bilinear\0");
          mat.filter.value = val == 0 ? 0 : 2;
        });
      });

      subSection("Values", [&]
      {
        toggleProp("Prim", mat.primColorSet.value, mat.primColor);
        toggleProp("Env", mat.envColorSet.value, mat.envColor);
        toggleProp("LOD", mat.primLodSet.value, mat.primLod);
        toggleProp("K4/K5", mat.k4k5Set.value, mat.k4k5);

        mat.primLod.value = glm::clamp(mat.primLod.value, 0u, 255u);
        mat.k4k5.value = glm::clamp(mat.k4k5.value, 0, 255);
      });

      subSection("Render Modes", [&]
      {
        ImTable::add("Blending"); ImGui::Text("@TODO");
        ImTable::add("Fog"); ImGui::Text("@TODO");

        ImTable::add("Vertex FX");
        ImGui::Combo("##Vert", &mat.vertexFX.value, VERTEX_EFFECTS);
        ImTable::addProp("Fog to Alpha", mat.fogToAlpha);

        toggleProp("Alpha-Clip", mat.alphaCompSet.value, [&] {
          ImGui::SliderInt("##AC", &mat.alphaComp.value, 0, 255,
            mat.alphaComp.value == 0 ? "<Off>" : "%d"
          );
        });

        toggleProp("Depth", mat.zmodeSet.value, [&] {
          ImGui::Combo("##", &mat.zmode.value, Z_MODES);
        });

        toggleProp("Fixed-Z", mat.zprimSet.value, [&] {
          sideBySide(
            [&]{ ImGui::InputInt("##0", &mat.zprim.value); },
            [&]{ ImGui::InputInt("##1", &mat.zdelta.value); }
          );
        });

        toggleProp("Anti-Alias", mat.aaSet.value, [&] {
          ImGui::Combo("##AA", &mat.aa.value, AA_MODES);
        });
      });

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
