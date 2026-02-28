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
      .moveForward = ImGuiKey_W,  
      .moveBack = ImGuiKey_S,  
      .moveLeft = ImGuiKey_A,  
      .moveRight = ImGuiKey_D,  
      .moveUp = ImGuiKey_E,  
      .moveDown = ImGuiKey_Q,  
      .toggleOrtho = ImGuiKey_Tab, 
      .focusObject = ImGuiKey_F,  
      .gizmoTranslate = ImGuiKey_W,  
      .gizmoRotate = ImGuiKey_E,  
      .gizmoScale = ImGuiKey_R,  
      .deleteObject = ImGuiKey_Delete,  
      .snapObject = ImGuiKey_S
  };
}