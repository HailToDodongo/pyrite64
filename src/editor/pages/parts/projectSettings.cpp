/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectSettings.h"

#include <algorithm>
#include <filesystem>
#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "../../../project/romMeta.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "IconsMaterialDesignIcons.h"

namespace
{
  using namespace Project;

  // True if an image asset reference cannot be embedded as-is (missing, wrong format,
  // or larger than the 320x240 / 240x320 limit enforced by n64metadata).
  bool imageRefInvalid(uint64_t uuid, std::string &reason)
  {
    if(uuid == 0) return false;
    auto *e = ctx.project->getAssets().getEntryByUUID(uuid);
    if(!e || e->type != FileType::IMAGE) { reason = "asset missing"; return true; }

    std::string ext = std::filesystem::path(e->path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if(ext != ".png" && ext != ".jpg" && ext != ".jpeg") { reason = "not PNG/JPG"; return true; }

    int w = e->texture ? e->texture->getWidth() : 0;
    int h = e->texture ? e->texture->getHeight() : 0;
    bool okSize = (w <= 320 && h <= 240) || (w <= 240 && h <= 320);
    if(w > 0 && !okSize) {
      reason = std::to_string(w) + "x" + std::to_string(h) + " exceeds 320x240";
      return true;
    }
    return false;
  }

  // Picks an IMAGE asset for `uuid`, with a clear button and an inline warning when invalid.
  // Returns true if the clear (X) button was pressed this frame, in addition to setting uuid=0.
  bool imagePicker(const std::string &label, uint64_t &uuid)
  {
    ImTable::add(label);
    ImGui::PushID(label.c_str());

    const auto &imgs = ctx.project->getAssets().getTypeEntries(FileType::IMAGE);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28_px);
    ImTable::addAssetVecComboBox("", imgs, uuid, true);
    ImGui::SameLine();
    bool cleared = ImGui::Button(ICON_MDI_CLOSE);
    if(cleared) uuid = 0;

    std::string reason;
    if(imageRefInvalid(uuid, reason)) {
      ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, ICON_MDI_ALERT " %s", reason.c_str());
    }
    ImGui::PopID();
    return cleared;
  }

  void drawMetaLang(MetaLang &lang)
  {
    ImTable::start("MetaFields");
    ImTable::add("Name", lang.name);
    ImTable::add("Author", lang.author);
    ImTable::add("Release Date", lang.releaseDate); // YYYY-MM-DD
    ImTable::add("OSI License", lang.osiLicense);    // SPDX code, e.g. MIT
    ImTable::add("Website", lang.website);
    ImTable::add("Age Rating", lang.ageRating);      // 0-18
    ImTable::add("Short Desc", lang.shortDesc);

    ImTable::add("Long Desc");
    ImGui::InputTextMultiline("##longdesc", &lang.longDesc, ImVec2(-FLT_MIN, 80_px));

    // Screenshots: variable-length list of image refs, the X removes the whole entry.
    int removeShot = -1;
    for(size_t i = 0; i < lang.screenshots.size(); ++i) {
      ImGui::PushID((int)i);
      if(imagePicker("Screenshot " + std::to_string(i + 1), lang.screenshots[i])) removeShot = (int)i;
      ImGui::PopID();
    }
    if(removeShot >= 0) lang.screenshots.erase(lang.screenshots.begin() + removeShot);
    ImTable::add("");
    if(ImGui::Button(ICON_MDI_PLUS " Screenshot")) lang.screenshots.push_back(0);

    ImTable::end();

    if(ImGui::CollapsingSubHeader("Box Art")) {
      ImTable::start("BoxArt");
      imagePicker("Front", lang.boxFront);
      imagePicker("Back", lang.boxBack);
      imagePicker("Top", lang.boxTop);
      imagePicker("Bottom", lang.boxBottom);
      imagePicker("Left", lang.boxLeft);
      imagePicker("Right", lang.boxRight);
      ImTable::end();
    }
    if(ImGui::CollapsingSubHeader("Cart Art")) {
      ImTable::start("CartArt");
      imagePicker("Front", lang.cartFront);
      imagePicker("Back", lang.cartBack);
      ImTable::end();
    }
  }

  void drawRomHeader()
  {
    auto &h = ctx.project->conf.romHeader;
    ImTable::start("RomHeader");
    ImTable::addComboBox("Category", h.category, RomMeta::labels(RomMeta::CATEGORY));
    ImTable::addComboBox("Region", h.region, RomMeta::labels(RomMeta::REGION));
    ImTable::addComboBox("Save Type", h.saveType, RomMeta::labels(RomMeta::SAVETYPE));
    ImTable::addCheckBox("Region Free", h.regionFree);
    ImTable::addCheckBox("Real-Time Clock", h.rtc);
    auto ctrlLabels = RomMeta::labels(RomMeta::CONTROLLER);
    for(int i = 0; i < 4; ++i) {
      ImTable::addComboBox("Controller " + std::to_string(i + 1), h.controllers[i], ctrlLabels);
    }
    ImTable::end();
  }

  void drawMetadata()
  {
    auto &m = ctx.project->conf.metadata;
    ImTable::start("MetadataTop");
    ImTable::addCheckBox("Embed Metadata", m.enabled);
    ImTable::end();
    if(!m.enabled) return;

    if(ImGui::BeginTabBar("MetaLangs")) {
      int removeLang = -1;
      for(size_t i = 0; i < m.langs.size(); ++i) {
        auto &lang = m.langs[i];
        std::string tabName = lang.lang.empty() ? "Default" : lang.lang;
        if(ImGui::BeginTabItem((tabName + "##" + std::to_string(i)).c_str())) {
          if(i > 0) {
            if(ImGui::SmallButton(ICON_MDI_DELETE " Remove Language")) removeLang = (int)i;
          }
          drawMetaLang(lang);
          ImGui::EndTabItem();
        }
      }

      // "+" tab opens a popup to enter a new language code.
      if(ImGui::TabItemButton(ICON_MDI_PLUS, ImGuiTabItemFlags_Trailing)) {
        ImGui::OpenPopup("AddLang");
      }
      if(ImGui::BeginPopup("AddLang")) {
        static std::string code;
        ImGui::TextUnformatted("Language code (e.g. de, ja):");
        ImGui::InputText("##code", &code);
        if(ImGui::Button("Add") && !code.empty()) {
          MetaLang l{};
          l.lang = code;
          m.langs.push_back(l);
          code.clear();
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if(removeLang > 0) m.langs.erase(m.langs.begin() + removeLang);
      ImGui::EndTabBar();
    }
  }

  // Validate every language's image refs (not just the visible tab) so Save can be blocked.
  bool metadataHasError()
  {
    if(!ctx.project->conf.metadata.enabled) return false;
    std::string reason;
    auto chk = [&](uint64_t u) { return imageRefInvalid(u, reason); };
    for(const auto &l : ctx.project->conf.metadata.langs) {
      if(chk(l.boxFront) || chk(l.boxBack) || chk(l.boxTop) || chk(l.boxBottom) || chk(l.boxLeft) || chk(l.boxRight)) return true;
      if(chk(l.cartFront) || chk(l.cartBack)) return true;
      for(auto u : l.screenshots) if(chk(u)) return true;
    }
    return false;
  }
}

bool Editor::ProjectSettings::draw()
{
  ImGui::BeginChild("TOP", ImVec2(0, -26_px));

  if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("General");
    ImTable::add("Name", ctx.project->conf.name);
    ImTable::add("ROM-Name", ctx.project->conf.romName);
    ImTable::end();
  }
  if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImTable::start("Collision");

    ImTable::add("Layer Names");
    for(int i=0; i<8; ++i) {
      ImTable::add("Layer " + std::to_string(i));
      ImGui::InputText(("##" + std::to_string(i)).c_str(), &ctx.project->conf.collLayerNames[i]);
    }
    ImTable::end();
  }

  if (ImGui::CollapsingHeader("ROM Header")) {
    drawRomHeader();
  }

  if (ImGui::CollapsingHeader("Metadata")) {
    drawMetadata();
  }

  if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Environment");
    ImTable::addPath("Emulator", ctx.project->conf.pathEmu);
    ImTable::addPath("N64_INST", ctx.project->conf.pathN64Inst, true, "$N64_INST");
    ImTable::end();
  }

  ImGui::EndChild();

  bool hasError = metadataHasError();

  ImGui::BeginChild("BOTTOM", ImVec2(0, 24_px));
    if(hasError) {
      ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, ICON_MDI_ALERT " Fix invalid metadata images before saving");
      ImGui::SameLine();
    }
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 64_px);

    if(hasError) ImGui::BeginDisabled();
    bool res = ImGui::Button("Save", ImVec2(60_px, 0));
    if(hasError) ImGui::EndDisabled();
  ImGui::EndChild();

  if (res) {
    ctx.project->save();
  }
  return res;
}
