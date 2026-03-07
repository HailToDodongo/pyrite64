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

inline ImGuiKeyChord GetKeyChordFromName(const std::string& name) {
    if (name.empty()) return ImGuiKey_None;
    ImGuiKeyChord chord = ImGuiKey_None;
    if (name.find("Ctrl+") != std::string::npos)  chord |= ImGuiMod_Ctrl;
    if (name.find("Cmd+") != std::string::npos)   chord |= ImGuiMod_Ctrl;
    if (name.find("Shift+") != std::string::npos) chord |= ImGuiMod_Shift;
    if (name.find("Alt+") != std::string::npos)   chord |= ImGuiMod_Alt;
    if (name.find("Super+") != std::string::npos) chord |= ImGuiMod_Super;

    std::string key_str = name;
    size_t last_plus = name.find_last_of('+');
    if (last_plus != std::string::npos) {
        key_str = name.substr(last_plus + 1); 
    }
    chord |= GetKeyFromName(key_str);
    return chord;
}

nlohmann::json Editor::Input::Keymap::serialize(KeymapPreset preset) const {
  Keymap defaultKeymap;
  if (preset == KeymapPreset::Blender) defaultKeymap = blenderKeymap;
  else if (preset == KeymapPreset::Standard) defaultKeymap = standardKeymap;

  Utils::JSON::Builder json = Utils::JSON::Builder{};
  auto writeKey = [&](const char* name, ImGuiKey currentKey, ImGuiKey defaultKey) {
    if (currentKey != defaultKey) json.set(name, ImGui::GetKeyName(currentKey));
  };
  auto writeChord = [&](const char* name, ImGuiKeyChord currentChord, ImGuiKeyChord defaultChord) {
    if (currentChord != defaultChord) json.set(name, GetKeyChordName(currentChord));
  };

  writeChord("zoomIn",       zoomIn,         defaultKeymap.zoomIn);
  writeChord("zoomOut",      zoomOut,        defaultKeymap.zoomOut);
  writeChord("save",         save,           defaultKeymap.save);
  writeChord("copy",         copy,           defaultKeymap.copy);
  writeChord("paste",        paste,          defaultKeymap.paste);
  writeChord("reloadAssets", reloadAssets,   defaultKeymap.reloadAssets);
  writeChord("build",        build,          defaultKeymap.build);
  writeChord("buildAndRun",  buildAndRun,    defaultKeymap.buildAndRun);
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
  Keymap defaultKeymap;
  if (preset == KeymapPreset::Blender) defaultKeymap = blenderKeymap;
  else if (preset == KeymapPreset::Standard) defaultKeymap = standardKeymap;

  auto readKey = [&](const char* fieldName, ImGuiKey defaultKey) {
    if (parent.is_null() || !parent.contains(fieldName)) return defaultKey;
    std::string name = parent[fieldName];
    ImGuiKey key = GetKeyFromName(name.c_str());
    return (key == ImGuiKey_None) ? defaultKey : key;
  };
   auto readChord = [&](const char* fieldName, ImGuiKeyChord defaultChord) {
    if (parent.is_null() || !parent.contains(fieldName)) return defaultChord;
    std::string name = parent[fieldName];
    ImGuiKeyChord chord = GetKeyChordFromName(name.c_str());
    return (chord == ImGuiKey_None) ? defaultChord : chord;
  };

  zoomIn         = readChord("zoomIn",       defaultKeymap.zoomIn);
  zoomOut        = readChord("zoomOut",      defaultKeymap.zoomOut);
  save           = readChord("save",         defaultKeymap.save);
  copy           = readChord("copy",         defaultKeymap.copy);
  paste          = readChord("paste",        defaultKeymap.paste);
  reloadAssets   = readChord("reloadAssets", defaultKeymap.reloadAssets);
  build          = readChord("build",        defaultKeymap.build);
  buildAndRun    = readChord("buildAndRun",  defaultKeymap.buildAndRun);
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

std::string Editor::Input::GetKeyChordName(ImGuiKeyChord key_chord)
{
  std::string result{};
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
