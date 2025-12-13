/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "../renderer/n64Mesh.h"
#include "../renderer/object.h"
#include "../utils/codeParser.h"
#include "../renderer/texture.h"
#include "tiny3d/tools/gltf_importer/src/structs.h"

namespace Project
{
  class Project;

  enum class ComprTypes : int
  {
    DEFAULT = 0,
    LEVEL_0,
    LEVEL_1,
    LEVEL_2,
    LEVEL_3,
  };

  class AssetManager
  {
    public:
      enum class FileType : int
      {
        UNKNOWN = 0,
        IMAGE,
        AUDIO,
        MODEL_3D,
        CODE_OBJ,
        CODE_GLOBAL,
        PREFAB,

        _SIZE
      };

      struct AssetConf
      {
        int format{0};
        int baseScale{0};
        bool gltfBVH{0};
        ComprTypes compression{ComprTypes::DEFAULT};
        bool exclude{false};

        std::string serialize() const;
      };

      struct Entry
      {
        uint64_t uuid{0};
        std::string name{};
        std::string path{};
        std::string outPath{};
        std::string romPath{};
        FileType type{};
        std::shared_ptr<Renderer::Texture> texture{nullptr};
        T3DMData t3dmData{};
        std::shared_ptr<Renderer::N64Mesh> mesh3D{};
        AssetConf conf{};
        Utils::CPP::Struct params{};
      };

    private:
      Project *project;
      std::array<std::vector<Entry>, static_cast<size_t>(FileType::_SIZE)> entries{};

      std::string defaultScript{};
      std::shared_ptr<Renderer::Texture> fallbackTex{};

      void reloadEntry(Entry &entry, const std::string &path);

    public:
      std::unordered_map<uint64_t, int> entriesMap{};
      //std::unordered_map<uint64_t, int> entriesMapScript{};

      AssetManager(Project *pr);
      ~AssetManager();

      void reload();
      void reloadAssetByUUID(uint64_t uuid);

      [[nodiscard]] const auto& getEntries() const {
        return entries;
      }
      [[nodiscard]] const std::vector<Entry>& getTypeEntries(FileType type) const {
        return entries[static_cast<int>(type)];
      }

      Entry* getByName(const std::string &name) {
        for (auto &typed : entries) {
          for (auto &entry : typed) {
            if (entry.name == name) {
              return &entry;
            }
          }
        }
        return nullptr;
      }

      Entry* getByPath(const std::string &path) {
        for (auto &typed : entries) {
          for (auto &entry : typed) {
            if (entry.path == path) {
              return &entry;
            }
          }
        }
        return nullptr;
      }

      Entry* getEntryByUUID(uint64_t uuid) {
        for (auto &typed : entries) {
          for (auto &entry : typed) {
            if (entry.uuid == uuid) {
              return &entry;
            }
          }
        }
        return nullptr;
      }

      const std::shared_ptr<Renderer::Texture> &getFallbackTexture();

      void save();

      void createScript(const std::string &name);
  };
}
