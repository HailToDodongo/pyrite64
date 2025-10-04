/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <vector>

#include "stringTable.h"
#include "../utils/binaryFile.h"

namespace Build
{
  struct SceneCtx
  {
    std::vector<std::string> files{};
    Utils::BinaryFile fileScene{};
    StringTable strTable{};
  };
}
