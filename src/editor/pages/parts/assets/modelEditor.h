/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>

#include "imgui.h"

namespace Editor
{
  class ModelEditor
  {
    private:
      uint64_t assetUUID{};
      std::string winName{};

    public:
      explicit ModelEditor(uint64_t assetUUID) : assetUUID(assetUUID) {}

      bool draw(ImGuiID defDockId);
      void focus() const;
  };
}
