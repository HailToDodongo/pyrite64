/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "preferences.h"

#include "../utils/prop.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"
#include "../utils/proc.h"

namespace fs = std::filesystem;

namespace
{
  fs::path getPrefsPath() {
    return Utils::Proc::getAppDataPath() / "preferences.json";
  }

  constexpr Editor::Preferences DEF{};
}

void Editor::Preferences::load()
{
  auto doc = Utils::JSON::loadFile(getPrefsPath());
  if(doc.is_object()) {
    keymapPreset = (Editor::Input::KeymapPreset)doc.value("keymapPreset", 0);
    if (doc.contains("keymap")) keymap.deserialize(doc["keymap"], keymapPreset);
    else applyKeymapPreset();

    zoomSpeed = doc.value("zoomSpeed", DEF.zoomSpeed);
    moveSpeed = doc.value("moveSpeed", DEF.moveSpeed);
    panSpeed = doc.value("panSpeed", DEF.panSpeed);
    lookSpeed = doc.value("lookSpeed", DEF.lookSpeed);
    invertWheelY = doc.value("invertWheelY", DEF.invertWheelY);
    renderFactorAA = doc.value("renderFactorAA", DEF.renderFactorAA);
  } else {
    applyKeymapPreset();
  }
}

void Editor::Preferences::save()
{
  std::string json = Utils::JSON::Builder{}
    .set("keymapPreset", (uint32_t)keymapPreset)
    .set("keymap", keymap.serialize(keymapPreset))
    .set("zoomSpeed", zoomSpeed)
    .set("moveSpeed", moveSpeed)
    .set("panSpeed", panSpeed)
    .set("lookSpeed", lookSpeed)
    .set("invertWheelY", invertWheelY)
    .set("renderFactorAA", renderFactorAA)
    .toString();
  auto prefPath = getPrefsPath();
  printf("Saving prefs to %s\n", prefPath.c_str());
  Utils::FS::saveTextFile(prefPath, json);
}

void Editor::Preferences::applyKeymapPreset()
{
  keymap = getCurrentKeymapPreset();
}

Editor::Input::Keymap Editor::Preferences::getCurrentKeymapPreset() const
{
  if (keymapPreset == Input::KeymapPreset::Blender) {
    return Input::blenderKeymap;
  }
  return Input::standardKeymap;
}
