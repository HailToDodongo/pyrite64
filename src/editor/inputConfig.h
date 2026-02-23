#pragma once
#include "imgui.h"

namespace Editor {
  enum class InputPreset {
    Blender,
    Standard
  };

  struct InputConfig {
    // Navigation/Viewport
    ImGuiKey moveForward    = ImGuiKey_W;
    ImGuiKey moveBack       = ImGuiKey_S;
    ImGuiKey moveLeft       = ImGuiKey_A;
    ImGuiKey moveRight      = ImGuiKey_D;
    ImGuiKey moveUp         = ImGuiKey_E;
    ImGuiKey moveDown       = ImGuiKey_Q;
    ImGuiKey toggleOrtho    = ImGuiKey_5;
    ImGuiKey focusObject    = ImGuiKey_F;

    // Gizmos
    ImGuiKey gizmoTranslate = ImGuiKey_G;
    ImGuiKey gizmoRotate    = ImGuiKey_R;
    ImGuiKey gizmoScale     = ImGuiKey_S;

    // Actions
    ImGuiKey deleteObject   = ImGuiKey_Delete;
    ImGuiKey snapObject     = ImGuiKey_S;

    void applyPreset(InputPreset preset) {
      if (preset == InputPreset::Blender) {
        gizmoTranslate = ImGuiKey_G;
        gizmoRotate    = ImGuiKey_R;
        gizmoScale     = ImGuiKey_S;
      } else if (preset == InputPreset::Standard) {
        gizmoTranslate = ImGuiKey_W;
        gizmoRotate    = ImGuiKey_E;
        gizmoScale     = ImGuiKey_R;
      }
    }
  };
}