/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>

#include "../renderer/texture.h"

namespace Project
{
  class AssetManager
  {
    public:
      enum class FileType
      {
        UNKNOWN = 0,
        IMAGE,
        AUDIO,
        MODEL_3D,
      };

      struct Entry
      {
        uint64_t uuid{0};
        std::string name{};
        std::string path{};
        FileType type{};
        Renderer::Texture *texture{nullptr};
      };

    private:
      std::vector<Entry> entries{};

    public:
      void reload();

      [[nodiscard]] const std::vector<Entry>& getEntries() const {
        return entries;
      }
  };
}
