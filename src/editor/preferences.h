/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include "keymap.h"

namespace Editor
{
  struct RecentProject
  {
    std::string path;      // absolute path to the .p64proj file
    std::string name;      // display name (project name at the time it was opened)
    std::string cardImage; // optional absolute path to the metadata cart/box art image
  };

  struct Preferences
  {
    Input::KeymapPreset keymapPreset{Input::KeymapPreset::Blender};
    Input::Keymap keymap{};
    std::string themeName{"dark"};
    std::vector<RecentProject> recentProjects{};
    float zoomSpeed = 1.0f;
    float moveSpeed = 120.0f;
    float panSpeed = 30.0f;
    float lookSpeed = -10.0f;
    bool invertWheelY = false;
    float renderFactorAA = 1.0f;
    bool useVSync = false;
    int fpsLimit = 60;
    bool showRotAsEuler = false;
    bool mouseWheelModifiesSpeed = false;
    bool viewportLockMode = false;

    void load();
    void save();

    void addRecentProject(const std::string &path, const std::string &name, const std::string &cardImage = "");
    void removeRecentProject(const std::string &path);

    void applyKeymapPreset();
    Input::Keymap getCurrentKeymapPreset() const;
  };
}
