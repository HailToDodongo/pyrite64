/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "memoryDashboard.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/string.h"
#include "../../imgui/theme.h"
#include <filesystem>

namespace fs = std::filesystem;

const char* Editor::MemoryDashboard::categoryName(AssetCategory cat)
{
  switch(cat) {
    case AssetCategory::Textures: return "Texture";
    case AssetCategory::Models:   return "Model";
    case AssetCategory::Audio:    return "Audio";
    case AssetCategory::Fonts:    return "Font";
    case AssetCategory::Scenes:   return "Scene";
    case AssetCategory::Code:     return "Code";
    case AssetCategory::Other:    return "Other";
    default: return "Unknown";
  }
}

ImVec4 Editor::MemoryDashboard::categoryColor(AssetCategory cat)
{
  switch(cat) {
    case AssetCategory::Textures: return {0.40f, 0.65f, 0.90f, 1.0f}; // blue
    case AssetCategory::Models:   return {0.45f, 0.78f, 0.45f, 1.0f}; // green
    case AssetCategory::Audio:    return {0.90f, 0.65f, 0.30f, 1.0f}; // orange
    case AssetCategory::Fonts:    return {0.70f, 0.50f, 0.85f, 1.0f}; // purple
    case AssetCategory::Scenes:   return {0.90f, 0.85f, 0.35f, 1.0f}; // yellow
    case AssetCategory::Code:     return {0.60f, 0.60f, 0.60f, 1.0f}; // gray
    case AssetCategory::Other:    return {0.50f, 0.50f, 0.50f, 1.0f}; // dark gray
    default: return {1.0f, 1.0f, 1.0f, 1.0f};
  }
}

void Editor::MemoryDashboard::scanBuildOutputs()
{
  entries.clear();
  totalRomSize = 0;
  for(int i = 0; i < static_cast<int>(AssetCategory::_COUNT); ++i) {
    categoryTotals[i] = 0;
  }

  if(!ctx.project) return;

  auto projectPath = fs::path{ctx.project->getPath()};
  auto fsPath = projectPath / "filesystem";

  // Check for the final .z64 ROM to get total size
  auto romPath = projectPath / (ctx.project->conf.romName + ".z64");
  if(fs::exists(romPath)) {
    totalRomSize = fs::file_size(romPath);
  }

  if(!fs::exists(fsPath)) return;

  // Cross-reference with asset manager for metadata
  auto &assets = ctx.project->getAssets();

  // Scan all files in filesystem/ recursively
  for(auto &dirEntry : fs::recursive_directory_iterator(fsPath)) {
    if(!dirEntry.is_regular_file()) continue;

    auto relPath = fs::relative(dirEntry.path(), fsPath).string();
    auto fileSize = dirEntry.file_size();

    // Determine category by cross-referencing with asset manager
    AssetCategory cat = AssetCategory::Other;
    std::string displayName = relPath;
    std::string comprStr = "-";

    // Try to match against known assets
    auto &allEntries = assets.getEntries();
    bool matched = false;
    for(int typeIdx = 0; typeIdx < static_cast<int>(Project::FileType::_SIZE); ++typeIdx) {
      for(auto &asset : allEntries[typeIdx]) {
        // Check if this filesystem file matches the asset's output path
        if(!asset.outPath.empty()) {
          // outPath is like "filesystem/foo/bar", relPath is "foo/bar"
          std::string outRel = asset.outPath;
          if(outRel.substr(0, 11) == "filesystem/") {
            outRel = outRel.substr(11);
          }
          if(relPath == outRel || relPath.find(outRel) != std::string::npos) {
            displayName = asset.name;
            matched = true;

            switch(asset.type) {
              case Project::FileType::IMAGE:       cat = AssetCategory::Textures; break;
              case Project::FileType::MODEL_3D:    cat = AssetCategory::Models; break;
              case Project::FileType::AUDIO:       cat = AssetCategory::Audio; break;
              case Project::FileType::FONT:        cat = AssetCategory::Fonts; break;
              case Project::FileType::CODE_OBJ:
              case Project::FileType::CODE_GLOBAL:
              case Project::FileType::NODE_GRAPH:  cat = AssetCategory::Code; break;
              case Project::FileType::PREFAB:      cat = AssetCategory::Other; break;
              default: break;
            }

            // Compression label
            switch(asset.conf.compression) {
              case Project::ComprTypes::DEFAULT: comprStr = "Default"; break;
              case Project::ComprTypes::LEVEL_0: comprStr = "None"; break;
              case Project::ComprTypes::LEVEL_1: comprStr = "Level 1"; break;
              case Project::ComprTypes::LEVEL_2: comprStr = "Level 2"; break;
              case Project::ComprTypes::LEVEL_3: comprStr = "Level 3"; break;
            }
            break;
          }
        }
      }
      if(matched) break;
    }

    // Heuristic fallback for unmatched files
    if(!matched) {
      if(relPath.find("p64/scenes/") != std::string::npos) {
        cat = AssetCategory::Scenes;
      }
    }

    categoryTotals[static_cast<int>(cat)] += fileSize;
    entries.push_back({displayName, cat, fileSize, comprStr});
  }

  hasData = true;
  sortEntries();
}

void Editor::MemoryDashboard::sortEntries()
{
  std::sort(entries.begin(), entries.end(), [this](const AssetEntry &a, const AssetEntry &b) {
    int cmp = 0;
    switch(sortColumn) {
      case 0: // Type
        cmp = strcmp(categoryName(a.category), categoryName(b.category));
        break;
      case 1: // Name
        cmp = a.name.compare(b.name);
        break;
      case 2: // Size
        cmp = (a.sizeBytes < b.sizeBytes) ? -1 : (a.sizeBytes > b.sizeBytes) ? 1 : 0;
        break;
      case 3: // Compression
        cmp = a.compression.compare(b.compression);
        break;
    }
    return sortAscending ? (cmp < 0) : (cmp > 0);
  });
}

void Editor::MemoryDashboard::refresh()
{
  scanBuildOutputs();
}

void Editor::MemoryDashboard::drawBudgetBar()
{
  uint64_t cartLimit = CART_SIZES[selectedCartSize];
  uint64_t usedSize = totalRomSize > 0 ? totalRomSize : 0;

  // Fall back to sum of scanned files if no ROM exists
  if(usedSize == 0) {
    for(int i = 0; i < static_cast<int>(AssetCategory::_COUNT); ++i) {
      usedSize += categoryTotals[i];
    }
  }

  float ratio = cartLimit > 0 ? static_cast<float>(usedSize) / static_cast<float>(cartLimit) : 0.0f;
  float percent = ratio * 100.0f;

  // Header text
  ImGui::Text("ROM: %s / %s  [%.1f%%]",
    Utils::byteSize(usedSize).c_str(),
    CART_LABELS[selectedCartSize],
    percent);

  // Stacked bar
  ImVec2 barStart = ImGui::GetCursorScreenPos();
  float barWidth = ImGui::GetContentRegionAvail().x;
  float barHeight = 20.0f;

  auto *drawList = ImGui::GetWindowDrawList();

  // Background
  drawList->AddRectFilled(barStart,
    {barStart.x + barWidth, barStart.y + barHeight},
    IM_COL32(40, 40, 45, 255), 3.0f);

  // Draw each category segment
  float xOffset = 0.0f;
  for(int i = 0; i < static_cast<int>(AssetCategory::_COUNT); ++i) {
    if(categoryTotals[i] == 0) continue;
    float segWidth = (static_cast<float>(categoryTotals[i]) / static_cast<float>(cartLimit)) * barWidth;
    if(segWidth < 1.0f) segWidth = 1.0f;

    ImVec4 col = categoryColor(static_cast<AssetCategory>(i));
    drawList->AddRectFilled(
      {barStart.x + xOffset, barStart.y},
      {barStart.x + xOffset + segWidth, barStart.y + barHeight},
      ImGui::ColorConvertFloat4ToU32(col),
      (i == 0) ? 3.0f : 0.0f
    );
    xOffset += segWidth;
  }

  // Over-budget warning line
  if(ratio > 1.0f) {
    float lineX = barStart.x + barWidth;
    drawList->AddLine({lineX, barStart.y}, {lineX, barStart.y + barHeight}, IM_COL32(255, 80, 80, 255), 2.0f);
  }

  ImGui::Dummy({barWidth, barHeight + 4.0f});
}

void Editor::MemoryDashboard::drawCategorySummary()
{
  bool first = true;
  for(int i = 0; i < static_cast<int>(AssetCategory::_COUNT); ++i) {
    if(categoryTotals[i] == 0) continue;

    if(!first) ImGui::SameLine();
    first = false;

    ImVec4 col = categoryColor(static_cast<AssetCategory>(i));
    ImGui::TextColored(col, "\xe2\x96\xa0");
    ImGui::SameLine();
    ImGui::Text("%s %s", categoryName(static_cast<AssetCategory>(i)), Utils::byteSize(categoryTotals[i]).c_str());
  }
  if(!first) ImGui::NewLine();
}

void Editor::MemoryDashboard::drawAssetTable()
{
  ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg
    | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
    | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

  if(!ImGui::BeginTable("##AssetTable", 4, flags)) return;

  ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultSort, 0.15f);
  ImGui::TableSetupColumn("Name", 0, 0.45f);
  ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 0.2f);
  ImGui::TableSetupColumn("Compression", 0, 0.2f);
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableHeadersRow();

  // Handle sorting
  if(auto *sortSpecs = ImGui::TableGetSortSpecs()) {
    if(sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0) {
      sortColumn = sortSpecs->Specs[0].ColumnIndex;
      sortAscending = (sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
      sortEntries();
      sortSpecs->SpecsDirty = false;
    }
  }

  for(auto &entry : entries) {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::TextColored(categoryColor(entry.category), "%s", categoryName(entry.category));

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(entry.name.c_str());

    ImGui::TableNextColumn();
    ImGui::Text("%s", Utils::byteSize(entry.sizeBytes).c_str());

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(entry.compression.c_str());
  }

  ImGui::EndTable();
}

void Editor::MemoryDashboard::draw()
{
  // Top toolbar: cart size selector + refresh button
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  ImGui::BeginChild("MEM_TOP", ImVec2(0, 22),
    ImGuiChildFlags_AlwaysUseWindowPadding,
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
  );
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

    ImGui::Text("Cart:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("##CartSize", &selectedCartSize, CART_LABELS, CART_SIZE_COUNT);

    ImGui::SameLine();
    if(ImGui::Button("Refresh", {64, 0})) {
      refresh();
    }

    ImGui::PopStyleVar(2);
  ImGui::EndChild();

  ImGui::PopStyleVar(2);

  if(!hasData) {
    ImGui::TextDisabled("No build data. Click Refresh after building your project.");
    return;
  }

  ImGui::Spacing();
  drawBudgetBar();
  drawCategorySummary();
  ImGui::Separator();
  drawAssetTable();
}
