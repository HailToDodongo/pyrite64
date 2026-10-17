/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "textureEditor.h"

#include "../../../../context.h"
#include "../../../imgui/helper.h"
#include "../../../undoRedo.h"
#include "IconsMaterialDesignIcons.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

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

namespace
{
  // "assets/a/b/x.png" -> "assets/a/b"
  std::string parentDir(const std::string &projectPath)
  {
    auto pos = projectPath.find_last_of('/');
    return pos == std::string::npos ? std::string{} : projectPath.substr(0, pos);
  }

  bool isInside(const std::string &dir, const std::string &root)
  {
    return dir == root || (dir.size() > root.size() && dir.starts_with(root) && dir[root.size()] == '/');
  }

  bool containsNoCase(const std::string &text, const std::string &needle)
  {
    if(needle.empty())return true;
    auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(), [](char a, char b) {
      return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
    });
    return it != text.end();
  }

  // Shortens a label with "..." so it fits into maxWidth.
  std::string fitText(const std::string &text, float maxWidth)
  {
    if(ImGui::CalcTextSize(text.c_str()).x <= maxWidth)return text;
    std::string res = text;
    while(!res.empty() && ImGui::CalcTextSize((res + "...").c_str()).x > maxWidth)res.pop_back();
    return res + "...";
  }

  struct PickerState
  {
    std::string dir{};
    std::string search{};
    bool recursive{true};
  };
}

bool Editor::TextureEditor::drawTexturePicker(const char *label, uint64_t &texUUID, const std::string &startDir)
{
  static PickerState state{};

  auto &assetManager = ctx.project->getAssets();
  const auto &images = assetManager.getTypeEntries(Project::FileType::IMAGE);
  const std::string rootDir = "assets";
  const std::string homeDir = startDir.empty() ? rootDir : startDir;

  const Project::AssetManagerEntry *current = nullptr;
  for(const auto &img : images) {
    if(img.getUUID() == texUUID) { current = &img; break; }
  }

  bool changed = false;
  bool locked = ImTable::isPrefabLocked();
  if(locked)ImGui::BeginDisabled();

  // Field: looks like a combo box, opens the picker
  std::string popupId = std::string{"##TexPicker"} + label;
  std::string preview = current ? current->name : std::string{"<None>"};
  float arrowW = ImGui::GetFrameHeight();
  float fieldW = ImGui::CalcItemWidth();
  if(ImGui::Button((fitText(preview, fieldW - arrowW - 12.0f) + "##" + label).c_str(), ImVec2(fieldW - arrowW, 0))) {
    state.dir = homeDir;
    state.search.clear();
    ImGui::OpenPopup(popupId.c_str());
  }
  if(current && ImGui::IsItemHovered())ImGui::SetTooltip("%s", current->projectPath.c_str());

  // Assets dragged from the browser onto the field
  if(ImGui::HandleComboBoxDragDrop(texUUID, [&images](uint64_t uuid, const char *type) {
    if(strcmp(type, "ASSET") != 0)return false;
    return std::any_of(images.begin(), images.end(), [uuid](const auto &img) { return img.getUUID() == uuid; });
  })) {
    changed = true;
  }

  ImGui::SameLine(0, 0);
  if(ImGui::ArrowButton((std::string{"##arrow"} + label).c_str(), ImGuiDir_Down)) {
    state.dir = homeDir;
    state.search.clear();
    ImGui::OpenPopup(popupId.c_str());
  }
  if(locked)ImGui::EndDisabled();

  ImGui::SetNextWindowSize(ImVec2(460_px, 420_px), ImGuiCond_Appearing);
  if(ImGui::BeginPopup(popupId.c_str()))
  {
    // --- Navigation bar: up, breadcrumbs, back to the model's folder
    ImGui::BeginDisabled(state.dir == rootDir);
    if(ImGui::Button(ICON_MDI_ARROW_UP)) {
      state.dir = parentDir(state.dir);
      if(state.dir.empty())state.dir = rootDir;
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("Up one folder");

    ImGui::SameLine();
    ImGui::BeginDisabled(state.dir == homeDir);
    if(ImGui::Button(ICON_MDI_HOME)) state.dir = homeDir;
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Back to %s", startDir.empty() ? "assets" : "the model's folder");
    }

    // breadcrumbs: every segment is a button that jumps there
    {
      std::string accum{};
      size_t start = 0;
      int seg = 0;
      while(start <= state.dir.size()) {
        auto end = state.dir.find('/', start);
        if(end == std::string::npos)end = state.dir.size();
        auto part = state.dir.substr(start, end - start);
        accum = accum.empty() ? part : accum + "/" + part;

        ImGui::SameLine(0, seg == 0 ? 8.0f : 2.0f);
        if(seg > 0) { ImGui::TextDisabled("/"); ImGui::SameLine(0, 2.0f); }
        ImGui::PushID(seg);
        if(ImGui::SmallButton(part.c_str()))state.dir = accum;
        ImGui::PopID();

        ++seg;
        if(end >= state.dir.size())break;
        start = end + 1;
      }
    }

    // --- Search + subfolder toggle
    if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-160_px);
    ImGui::InputTextWithHint("##search", ICON_MDI_MAGNIFY " Search this folder...", &state.search);
    ImGui::SameLine();
    ImGui::Checkbox("Include subfolders", &state.recursive);
    ImGui::Separator();

    // --- Collect folders and images for the current directory
    std::set<std::string> subDirs{};
    std::vector<const Project::AssetManagerEntry*> shown{};
    bool searching = !state.search.empty();
    for(const auto &img : images)
    {
      auto dir = parentDir(img.projectPath);
      if(!isInside(dir, state.dir))continue;

      if(dir != state.dir) {
        // first path segment below the current folder
        auto rest = dir.substr(state.dir.size() + 1);
        subDirs.insert(rest.substr(0, rest.find('/')));
      }

      bool inScope = (dir == state.dir) || state.recursive || searching;
      if(!inScope)continue;
      if(searching && !containsNoCase(img.name, state.search))continue;
      shown.push_back(&img);
    }
    std::sort(shown.begin(), shown.end(), [](auto *a, auto *b) { return a->projectPath < b->projectPath; });

    ImGui::BeginChild("##content", ImVec2(0, 0));

    // folders
    if(!searching) {
      for(const auto &sub : subDirs) {
        if(ImGui::Selectable((std::string{ICON_MDI_FOLDER " "} + sub).c_str())) {
          state.dir += "/" + sub;
        }
      }
      if(!subDirs.empty() && !shown.empty())ImGui::Separator();
    }

    if(shown.empty()) {
      ImGui::TextDisabled(searching ? "No textures match." : "No textures here.");
    }

    // image grid, only visible rows are drawn (and only those textures get uploaded)
    const float tile = 72_px;
    const float labelH = ImGui::GetTextLineHeight();
    const float cellW = tile + 8_px;
    const float cellH = tile + labelH + 10_px;
    int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellW));
    int rows = ((int)shown.size() + cols - 1) / cols;

    ImGuiListClipper clipper;
    clipper.Begin(rows, cellH);
    while(clipper.Step())
    {
      for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
      {
        ImVec2 rowPos = ImGui::GetCursorScreenPos();
        for(int col = 0; col < cols; ++col)
        {
          int idx = row * cols + col;
          if(idx >= (int)shown.size())break;
          const auto *img = shown[idx];

          ImVec2 p0{rowPos.x + col * cellW, rowPos.y};
          ImGui::SetCursorScreenPos(p0);
          ImGui::PushID(idx);
          bool clicked = ImGui::InvisibleButton("##tile", ImVec2(cellW - 4_px, cellH - 4_px));
          bool hovered = ImGui::IsItemHovered();
          ImGui::PopID();

          auto *dl = ImGui::GetWindowDrawList();
          bool isSel = img->getUUID() == texUUID;
          if(isSel || hovered) {
            dl->AddRectFilled(p0, ImVec2(p0.x + cellW - 4_px, p0.y + cellH - 4_px),
              ImGui::GetColorU32(isSel ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), 4.0f);
          }

          // thumbnail, aspect-correct inside the tile
          if(img->texture && img->texture->getWidth() > 0) {
            float w = (float)img->texture->getWidth();
            float h = (float)img->texture->getHeight();
            float scale = tile / std::max(w, h);
            ImVec2 size{w * scale, h * scale};
            ImVec2 imgP0{p0.x + 2_px + (tile - size.x) * 0.5f, p0.y + 2_px + (tile - size.y) * 0.5f};
            dl->AddImage(ImTextureRef(img->texture->getGPUTex()), imgP0, ImVec2(imgP0.x + size.x, imgP0.y + size.y));
          }

          // name without extension, shortened to fit
          std::string name = img->name;
          if(auto dot = name.find('.'); dot != std::string::npos)name = name.substr(0, dot);
          name = fitText(name, cellW - 8_px);
          dl->AddText(ImVec2(p0.x + 2_px, p0.y + tile + 4_px), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());

          if(hovered) {
            ImGui::SetTooltip("%s\n%dx%d", img->projectPath.c_str(),
              img->texture ? img->texture->getWidth() : 0, img->texture ? img->texture->getHeight() : 0);
          }
          if(clicked) {
            if(texUUID != img->getUUID()) {
              texUUID = img->getUUID();
              changed = true;
            }
            ImGui::CloseCurrentPopup();
          }
        }
        // a real item for the row, so the child knows its size (no SetCursorPos-only extent)
        ImGui::SetCursorScreenPos(rowPos);
        ImGui::Dummy(ImVec2(cols * cellW, cellH));
      }
    }
    clipper.End();

    ImGui::EndChild();
    ImGui::EndPopup();
  }

  if(changed) {
    Editor::UndoRedo::getHistory().markChanged(std::string{"Edit "} + label);
  }
  return changed;
}

void Editor::TextureEditor::draw(Project::Assets::MaterialTex &tex, uint64_t modelUUID)
{
  auto &assetManager = ctx.project->getAssets();

  // start the picker in the folder of the model this material belongs to
  std::string startDir{};
  if(modelUUID) {
    if(auto model = assetManager.getEntryByUUID(modelUUID)) {
      startDir = parentDir(model->projectPath);
    }
  }

  ImTable::add("Texture");
  drawTexturePicker("Texture", tex.texUUID.value, startDir);

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
    if(imgSize.x >= imgSize.y)
    {
      imgSize.x = maxWidth;
      imgSize.y = maxWidth / imgRatio;
    } else {
      imgSize.y = maxWidth;
      imgSize.x = maxWidth * imgRatio;
    }

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
