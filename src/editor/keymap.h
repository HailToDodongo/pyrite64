/**
* @copyright 2026 - Nolan Baker
* @license MIT
*/
#pragma once
#include "imgui.h"
#include "json.hpp"

namespace Editor::Input {

  enum class KeymapPreset {
    Blender,
    Standard
  };

  struct Keymap {
    // Window
    ImGuiKeyChord zoomIn       = ImGuiMod_Ctrl | ImGuiKey_MouseWheelY;
    ImGuiKeyChord zoomOut      = ImGuiMod_Ctrl | ImGuiKey_MouseWheelY;

    // Project
    ImGuiKeyChord save         = ImGuiMod_Ctrl | ImGuiKey_S;
    ImGuiKeyChord copy         = ImGuiMod_Ctrl | ImGuiKey_C;
    ImGuiKeyChord paste        = ImGuiMod_Ctrl | ImGuiKey_V;
    ImGuiKeyChord reloadAssets = ImGuiKey_F5;
    ImGuiKeyChord build        = ImGuiKey_F11;
    ImGuiKeyChord buildAndRun  = ImGuiKey_F12;

    // Navigation/Viewport
    ImGuiKey moveForward    = ImGuiKey_W;
    ImGuiKey moveBack       = ImGuiKey_S;
    ImGuiKey moveLeft       = ImGuiKey_A;
    ImGuiKey moveRight      = ImGuiKey_D;
    ImGuiKey moveUp         = ImGuiKey_E;
    ImGuiKey moveDown       = ImGuiKey_Q;
    ImGuiKey toggleOrtho    = ImGuiKey_5;
    ImGuiKey focusObject    = ImGuiKey_Period;

    // Gizmos
    ImGuiKey gizmoTranslate = ImGuiKey_G;
    ImGuiKey gizmoRotate    = ImGuiKey_R;
    ImGuiKey gizmoScale     = ImGuiKey_S;

    // Actions
    ImGuiKey deleteObject   = ImGuiKey_Delete;
    ImGuiKey snapObject     = ImGuiKey_S;

    nlohmann::json serialize(KeymapPreset preset) const;
    void deserialize(const nlohmann::json& parent, KeymapPreset preset);
  };

  static Keymap blenderKeymap;
  static Keymap standardKeymap = (Keymap) {
    .reloadAssets     = ImGuiMod_Ctrl | ImGuiKey_R,
    .build            = ImGuiMod_Ctrl | ImGuiKey_B,
    .buildAndRun      = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B,
    .toggleOrtho      = ImGuiKey_Tab, 
    .focusObject      = ImGuiKey_F,  
    .gizmoTranslate   = ImGuiKey_W,  
    .gizmoRotate      = ImGuiKey_E,  
    .gizmoScale       = ImGuiKey_R,
  };

  std::string GetKeyChordName(ImGuiKeyChord key_chord);
}
