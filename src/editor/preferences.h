/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "keymap.h"

namespace Editor
{
  struct Preferences
  {
    Input::KeymapPreset keymapPreset{Input::KeymapPreset::Blender};
    Input::Keymap keymap{};
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

    void load();
    void save();

    void applyKeymapPreset();
    Input::Keymap getCurrentKeymapPreset() const;
  };
}
