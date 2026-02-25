/**
* @copyright 2026 - Nolan Baker
* @license MIT
*/
#include "keymap.h"

#include <string>
#include "imgui.h"
#include "../utils/prop.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"

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

nlohmann::json Editor::Input::Keymap::serialize(KeymapPreset preset) const {
  Keymap defaultKeymap;
  if (preset == KeymapPreset::Blender) defaultKeymap = blenderKeymap;
  else if (preset == KeymapPreset::Standard) defaultKeymap = standardKeymap;

  Utils::JSON::Builder json = Utils::JSON::Builder{};
  auto writeKey = [&](const char* name, ImGuiKey currentKey, ImGuiKey defaultKey) {
    if (currentKey != defaultKey) json.set(name, ImGui::GetKeyName(currentKey));
  };
  
  writeKey("moveForward",    moveForward,    defaultKeymap.moveForward);
  writeKey("moveBack",       moveBack,       defaultKeymap.moveBack);
  writeKey("moveLeft",       moveLeft,       defaultKeymap.moveLeft);
  writeKey("moveRight",      moveRight,      defaultKeymap.moveRight);
  writeKey("moveUp",         moveUp,         defaultKeymap.moveUp);
  writeKey("moveDown",       moveDown,       defaultKeymap.moveDown);
  writeKey("toggleOrtho",    toggleOrtho,    defaultKeymap.toggleOrtho);
  writeKey("focusObject",    focusObject,    defaultKeymap.focusObject);
  writeKey("gizmoTranslate", gizmoTranslate, defaultKeymap.gizmoTranslate);
  writeKey("gizmoRotate",    gizmoRotate,    defaultKeymap.gizmoRotate);
  writeKey("gizmoScale",     gizmoScale,     defaultKeymap.gizmoScale);
  writeKey("deleteObject",   deleteObject,   defaultKeymap.deleteObject);
  writeKey("snapObject",     snapObject,     defaultKeymap.snapObject);
  return json.doc;
}

void Editor::Input::Keymap::deserialize(const nlohmann::json& parent, KeymapPreset preset) {
  if (parent.is_null()) return;

  Keymap defaultKeymap;
  if (preset == KeymapPreset::Blender) defaultKeymap = blenderKeymap;
  else if (preset == KeymapPreset::Standard) defaultKeymap = standardKeymap;

  auto readKey = [&](const char* fieldName, ImGuiKey defaultKey) {
  if (!parent.contains(fieldName)) return defaultKey;
  std::string name = parent[fieldName];
  ImGuiKey key = GetKeyFromName(name.c_str());
  return (key == ImGuiKey_None) ? defaultKey : key;
  };

  moveForward    = readKey("moveForward",    defaultKeymap.moveForward);
  moveBack       = readKey("moveBack",       defaultKeymap.moveBack);
  moveLeft       = readKey("moveLeft",       defaultKeymap.moveLeft);
  moveRight      = readKey("moveRight",      defaultKeymap.moveRight);
  moveUp         = readKey("moveUp",         defaultKeymap.moveUp);
  moveDown       = readKey("moveDown",       defaultKeymap.moveDown);
  toggleOrtho    = readKey("toggleOrtho",    defaultKeymap.toggleOrtho);
  focusObject    = readKey("focusObject",    defaultKeymap.focusObject);
  gizmoTranslate = readKey("gizmoTranslate", defaultKeymap.gizmoTranslate);
  gizmoRotate    = readKey("gizmoRotate",    defaultKeymap.gizmoRotate);
  gizmoScale     = readKey("gizmoScale",     defaultKeymap.gizmoScale);
  deleteObject   = readKey("deleteObject",   defaultKeymap.deleteObject);
  snapObject     = readKey("snapObject",     defaultKeymap.snapObject);
}
