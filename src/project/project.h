/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <array>
#include <vector>
#include <cstdint>

#include "assetManager.h"
#include "scene/sceneManager.h"

namespace Project
{
  // N64 ROM header / EverDrive config. Indices map into the option tables in romMeta.h,
  // index 0 is the default for category/region (no flag emitted).
  struct RomHeaderConf
  {
    int category{0};
    int region{0};
    int saveType{0};
    bool regionFree{true};
    bool rtc{false};
    std::array<int, 4> controllers{0, 0, 0, 0};
  };

  // One language entry of the embedded ROM metadata (libdragon n64metadata / Homebrew Header).
  // lang is empty for the default [meta] section, otherwise a code like "de" -> [meta.de].
  // Image fields hold IMAGE-asset UUIDs (0 = unset).
  struct MetaLang
  {
    std::string lang{};
    std::string name{};
    std::string author{};
    std::string releaseDate{};
    std::string osiLicense{};
    std::string website{};
    std::string shortDesc{};
    std::string longDesc{};
    int ageRating{0};
    std::vector<uint64_t> screenshots{};
    uint64_t boxFront{}, boxBack{}, boxTop{}, boxBottom{}, boxLeft{}, boxRight{};
    uint64_t cartFront{}, cartBack{};
  };

  struct MetadataConf
  {
    bool enabled{false};
    std::vector<MetaLang> langs{MetaLang{}}; // langs[0] is always the default (unsuffixed) section
  };

  struct ProjectConf
  {
    std::string name{};
    std::string romName{};
    std::string pathEmu{};
    std::string pathN64Inst{};

    // Editor version this project was last saved with (empty for pre-versioning projects).
    std::string editorVersion{};

    RomHeaderConf romHeader{};
    MetadataConf metadata{};

    uint32_t sceneIdOnBoot{1};
    uint32_t sceneIdOnReset{1};
    uint32_t sceneIdLastOpened{1};

    std::array<std::string, 8> collLayerNames{};

    std::string serialize() const;
  };

  class Project
  {
    private:
      std::string path;
      std::string pathConfig;
      bool dirty{false};
      std::string savedState{};
      bool openedFromNewerVersion{false};

      AssetManager assets{this};
      SceneManager scenes{this};

      void deserialize(const nlohmann::json &doc);

    public:
      ProjectConf conf{};

      Project(const std::string &p64projPath);

      void save();
      void saveConfig();
      void markDirty() { dirty = true; }
      void markSaved() { dirty = false; savedState = conf.serialize(); }
      [[nodiscard]] bool isDirty() const { return dirty || conf.serialize() != savedState || assets.isDirty(); }

      AssetManager& getAssets() { return assets; }
      SceneManager& getScenes() { return scenes; }
      [[nodiscard]] const std::string &getPath() const { return path; }
      [[nodiscard]] const std::string &getConfigPath() const { return pathConfig; }
      [[nodiscard]] bool wasSavedWithNewerVersion() const { return openedFromNewerVersion; }
  };
}