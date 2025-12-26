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
#include "scene/prefab.h"
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
        FONT,
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

        PROP_BOOL(wavForceMono);
        PROP_U32(wavResampleRate);
        PROP_S32(wavCompression);

        PROP_U32(fontId);
        PROP_STRING(fontCharset);

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
        T3DM::T3DMData t3dmData{};
        std::shared_ptr<Renderer::N64Mesh> mesh3D{};
        std::shared_ptr<Prefab> prefab{nullptr};
        AssetConf conf{};
        Utils::CPP::Struct params{};

        // imgui selectbox:
        uint64_t getId() const { return uuid; }
        const std::string &getName() const { return name; }
      };

    private:
      Project *project;
      std::array<std::vector<Entry>, static_cast<size_t>(FileType::_SIZE)> entries{};

      std::string defaultScript{};
      std::shared_ptr<Renderer::Texture> fallbackTex{};

      void reloadEntry(Entry &entry, const std::string &path);
    public:
      std::unordered_map<uint64_t, std::pair<int, int>> entriesMap{};
      //std::unordered_map<uint64_t, int> entriesMapScript{};

      explicit AssetManager(Project *pr);
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
        auto it = entriesMap.find(uuid);
        if (it == entriesMap.end()) {
          return nullptr;
        }
        return &entries[it->second.first][it->second.second];
      }

      std::shared_ptr<Prefab> getPrefabByUUID(uint64_t uuid) {
        auto entry = getEntryByUUID(uuid);
        if (!entry || entry->type != FileType::PREFAB) {
          return nullptr;
        }
        return entry->prefab;
      }

      const std::shared_ptr<Renderer::Texture> &getFallbackTexture();

      void save();

      void createScript(const std::string &name);
  };
}
