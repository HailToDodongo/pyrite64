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
    // Project
    ImGuiKeyChord save         = ImGuiMod_Ctrl | ImGuiKey_S;
    ImGuiKeyChord copy         = ImGuiMod_Ctrl | ImGuiKey_C;
    ImGuiKeyChord paste        = ImGuiMod_Ctrl | ImGuiKey_V;
    ImGuiKeyChord toggleVSync  = ImGuiKey_F2;
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

  static std::string GetKeyChordName(ImGuiKeyChord key_chord) {
    std::string result;
    ImGuiKey key = (ImGuiKey)(key_chord & ~ImGuiMod_Mask_);

    if (key_chord & ImGuiMod_Ctrl) {
  #if defined(__APPLE__)
      result += "Cmd+";
  #else
      result += "Ctrl+";
  #endif
    }
    if (key_chord & ImGuiMod_Shift) result += "Shift+";
    if (key_chord & ImGuiMod_Alt)   result += "Alt+";
    if (key_chord & ImGuiMod_Super) result += "Super+";

    // Append the base key name
    if (key != ImGuiKey_None || key_chord == ImGuiKey_None) {
      const char* key_name = ImGui::GetKeyName(key);
      if (key_name) result += key_name;
    } else if (!result.empty()) {
      result.pop_back();
    }
    return result;
  }
}