/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#include "assetManager.h"
#include "scene/sceneManager.h"
#include "../editor/keymap.h"//TODO: move keymap into preferences

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
    
    uint32_t keymapPreset{0};
    Editor::Input::Keymap keymap{};

    std::string serialize() const;
  };

  class Project
  {
    private:
      std::string path;
      std::string pathConfig;
      bool dirty{false};
      std::string savedState{};

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
      [[nodiscard]] bool isDirty() const { return dirty || conf.serialize() != savedState; }

      AssetManager& getAssets() { return assets; }
      SceneManager& getScenes() { return scenes; }
      [[nodiscard]] const std::string &getPath() const { return path; }
      [[nodiscard]] const std::string &getConfigPath() const { return pathConfig; }

  };
}