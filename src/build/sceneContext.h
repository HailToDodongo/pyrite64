/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>

#include "stringTable.h"
#include "../utils/binaryFile.h"

namespace Project { class Project; }

namespace Build
{
  struct SceneCtx
  {
    Project::Project *project{};
    std::vector<std::string> files{};
    std::array<uint64_t, 16> autoLoadFontUUIDs{};
    std::unordered_map<uint64_t, uint32_t> assetUUIDToIdx{};
    std::unordered_map<uint64_t, uint32_t> codeIdxMapUUID{};
    Utils::BinaryFile fileScene{};
    Utils::BinaryFile fileObj{};
    StringTable strTable{};
  };
}
