/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

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
    NODE_GRAPH,

    _SIZE
  };

  struct AssetConf
  {
    uint64_t uuid{0};
    int format{0};
    int baseScale{0};
    bool gltfBVH{0};
    PROP_BOOL(gltfCollision);

    ComprTypes compression{ComprTypes::DEFAULT};
    bool exclude{false};

    PROP_BOOL(wavForceMono);
    PROP_U32(wavResampleRate);
    PROP_S32(wavCompression);

    PROP_U32(fontId);
    PROP_STRING(fontCharset);

    std::string serialize() const;
  };

  struct AssetManagerEntry
  {
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

    uint64_t getUUID() const { return conf.uuid; }

    // imgui selectbox:
    uint64_t getId() const { return conf.uuid; }
    const std::string &getName() const { return name; }
  };

  class AssetManager
  {
    private:
      Project *project;
      std::array<std::vector<AssetManagerEntry>, static_cast<size_t>(FileType::_SIZE)> entries{};

      std::unordered_map<std::string, uint64_t> watchFiles{};
      std::chrono::steady_clock::time_point watchLastCheck{};
      bool watchInitialized{false};

      std::unordered_set<uint64_t> dirtyPrefabs{};
      std::unordered_set<uint64_t> dirtyAssetMeta{};
      std::unordered_set<uint64_t> dirtyNodeGraphs{};
      std::unordered_map<uint64_t, std::string> savedPrefabState{};
      std::unordered_map<uint64_t, std::string> savedAssetMetaState{};
      std::unordered_map<uint64_t, std::string> savedNodeGraphState{};
      std::unordered_map<uint64_t, std::string> dirtyNodeGraphState{};

      std::string defaultScript{};
      std::shared_ptr<Renderer::Texture> fallbackTex{};

      void reloadEntry(AssetManagerEntry &entry, const std::string &path);
      void resetDirtyTracking();
      void clearDirtyTracking(uint64_t uuid);
    public:
      std::unordered_map<uint64_t, std::pair<int, int>> entriesMap{};
      //std::unordered_map<uint64_t, int> entriesMapScript{};

      explicit AssetManager(Project *pr);
      ~AssetManager();

      void reload();
      void reloadAssetByUUID(uint64_t uuid);
      bool pollWatch();
      bool isDirty() const {
        return !dirtyPrefabs.empty() || !dirtyAssetMeta.empty() || !dirtyNodeGraphs.empty();
      }
      bool isNodeGraphDirty(uint64_t uuid) const {
        return dirtyNodeGraphs.contains(uuid);
      }

      [[nodiscard]] const auto& getEntries() const {
        return entries;
      }
      [[nodiscard]] const std::vector<AssetManagerEntry>& getTypeEntries(FileType type) const {
        return entries[static_cast<int>(type)];
      }

      AssetManagerEntry* getByName(const std::string &name) {
        for (auto &typed : entries) {
          for (auto &entry : typed) {
            if (entry.name == name) {
              return &entry;
            }
          }
        }
        return nullptr;
      }

      AssetManagerEntry* getByPath(const std::string &path);

      AssetManagerEntry* getEntryByUUID(uint64_t uuid) {
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

      void markPrefabDirty(uint64_t uuid);
      void markAssetMetaDirty(uint64_t uuid);
      void markNodeGraphDirty(uint64_t uuid, const std::string &currentState);
      void markNodeGraphSaved(uint64_t uuid, const std::string &savedState);
      void clearNodeGraphDirty(uint64_t uuid);

      void save();

      bool createScript(const std::string &name, const std::string &subDir = {});
      uint64_t createNodeGraph(const std::string &name);
  };
}
