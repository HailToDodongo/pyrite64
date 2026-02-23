/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "imgui.h"

namespace Editor
{
  class MemoryDashboard
  {
    public:
      void draw();
      void refresh();

    private:
      enum class AssetCategory : int {
        Textures = 0,
        Models,
        Audio,
        Fonts,
        Scenes,
        Code,
        Other,
        _COUNT
      };

      struct AssetEntry {
        std::string name{};
        AssetCategory category{};
        uint64_t sizeBytes{0};
        std::string compression{};
      };

      static constexpr int CART_SIZE_COUNT = 4;
      static constexpr uint64_t CART_SIZES[CART_SIZE_COUNT] = {
        8 * 1024 * 1024,
        16 * 1024 * 1024,
        32 * 1024 * 1024,
        64 * 1024 * 1024,
      };
      static constexpr const char* CART_LABELS[CART_SIZE_COUNT] = {
        "8 MB", "16 MB", "32 MB", "64 MB"
      };

      int selectedCartSize{3}; // default 64 MB
      uint64_t totalRomSize{0};
      uint64_t categoryTotals[static_cast<int>(AssetCategory::_COUNT)]{};
      std::vector<AssetEntry> entries{};
      int sortColumn{2}; // default sort by size
      bool sortAscending{false}; // default descending (largest first)
      bool hasData{false};

      void scanBuildOutputs();
      void sortEntries();
      void drawBudgetBar();
      void drawCategorySummary();
      void drawAssetTable();

      static const char* categoryName(AssetCategory cat);
      static ImVec4 categoryColor(AssetCategory cat);
  };
}
