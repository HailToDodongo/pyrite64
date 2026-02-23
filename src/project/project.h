/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#include "assetManager.h"
#include "scene/sceneManager.h"

namespace Project
{
  struct ProjectConf
  {
    std::string name{};
    std::string romName{};
    std::string pathEmu{};
    std::string pathN64Inst{};

    uint32_t sceneIdOnBoot{1};
    uint32_t sceneIdOnReset{1};
    uint32_t sceneIdLastOpened{1};

    std::string serialize() const;
  };

  class Project
  {
    private:
      std::string path;
      std::string pathConfig;

      AssetManager assets{this};
      SceneManager scenes{this};

      void deserialize(const nlohmann::json &doc);

    public:
      ProjectConf conf{};

      Project(const std::string &p64projPath);

      void save();
      void saveConfig();

      AssetManager& getAssets() { return assets; }
      SceneManager& getScenes() { return scenes; }
      [[nodiscard]] const std::string &getPath() const { return path; }
      [[nodiscard]] const std::string &getConfigPath() const { return pathConfig; }

  };
}