#pragma once
#include "imgui.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"

namespace Editor {
  inline ImGuiKey GetKeyFromName(const std::string& name) {
    if (name.empty()) return ImGuiKey_None;
    for (int n = ImGuiKey_NamedKey_BEGIN; n < ImGuiKey_NamedKey_END; n++) {
      ImGuiKey key = (ImGuiKey)n;
      const char* keyName = ImGui::GetKeyName(key);
      if (keyName && name == keyName) {
        return key;
      }
    }
    return ImGuiKey_None;
  }

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

    nlohmann::json  serialize() const {
      return Utils::JSON::Builder{}
        .set("moveForward", ImGui::GetKeyName(moveForward))
        .set("moveBack", ImGui::GetKeyName(moveBack))
        .set("moveLeft", ImGui::GetKeyName(moveLeft))
        .set("moveRight", ImGui::GetKeyName(moveRight))
        .set("moveUp", ImGui::GetKeyName(moveUp))
        .set("moveDown", ImGui::GetKeyName(moveDown))
        .set("toggleOrtho", ImGui::GetKeyName(toggleOrtho))
        .set("focusObject", ImGui::GetKeyName(focusObject))
        .set("gizmoTranslate", ImGui::GetKeyName(gizmoTranslate))
        .set("gizmoRotate", ImGui::GetKeyName(gizmoRotate))
        .set("gizmoScale", ImGui::GetKeyName(gizmoScale))
        .set("deleteObject", ImGui::GetKeyName(deleteObject))
        .set("snapObject", ImGui::GetKeyName(snapObject))
        .doc;
    }

    void deserialize(const nlohmann::json& parent) {
      if (parent.is_null()) return;

      auto readKey = [&](const char* fieldName, ImGuiKey defaultKey) {
        if (!parent.contains(fieldName)) return defaultKey;
        std::string name = parent[fieldName];
        ImGuiKey key = GetKeyFromName(name.c_str());
        return (key == ImGuiKey_None) ? defaultKey : key;
      };

      moveForward    = readKey("moveForward",    moveForward);
      moveBack       = readKey("moveBack",       moveBack);
      moveLeft       = readKey("moveLeft",       moveLeft);
      moveRight      = readKey("moveRight",      moveRight);
      moveUp         = readKey("moveUp",         moveUp);
      moveDown       = readKey("moveDown",       moveDown);
      focusObject    = readKey("focusObject",    focusObject);
      gizmoTranslate = readKey("gizmoTranslate", gizmoTranslate);
      gizmoRotate    = readKey("gizmoRotate",    gizmoRotate);
      gizmoScale     = readKey("gizmoScale",     gizmoScale);
      deleteObject   = readKey("deleteObject",   deleteObject);
      snapObject     = readKey("snapObject",     snapObject);
    }
  };
}