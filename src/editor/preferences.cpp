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

  const Editor::Preferences DEF{};
}

void Editor::Preferences::load()
{
  auto doc = Utils::JSON::loadFile(getPrefsPath());
  if(doc.is_object()) {
    keymapPreset = (Editor::Input::KeymapPreset)doc.value("keymapPreset", 0);
    if (doc.contains("keymap")) keymap.deserialize(doc["keymap"], keymapPreset);
    else applyKeymapPreset();

    themeName = doc.value("themeName", DEF.themeName);
    recentProjects.clear();
    if (doc.contains("recentProjects") && doc["recentProjects"].is_array()) {
      for (const auto &e : doc["recentProjects"]) {
        recentProjects.push_back({e.value("path", ""), e.value("name", ""), e.value("cardImage", "")});
      }
    }
    zoomSpeed = doc.value("zoomSpeed", DEF.zoomSpeed);
    moveSpeed = doc.value("moveSpeed", DEF.moveSpeed);
    panSpeed = doc.value("panSpeed", DEF.panSpeed);
    lookSpeed = doc.value("lookSpeed", DEF.lookSpeed);
    invertWheelY = doc.value("invertWheelY", DEF.invertWheelY);
    renderFactorAA = doc.value("renderFactorAA", DEF.renderFactorAA);
    useVSync = doc.value("useVSync", DEF.useVSync);
    fpsLimit = doc.value("fpsLimit", DEF.fpsLimit);
    showRotAsEuler = doc.value("showRotAsEuler", DEF.showRotAsEuler);
    mouseWheelModifiesSpeed = doc.value("mouseWheelModifiesSpeed", DEF.mouseWheelModifiesSpeed);
    viewportLockMode = doc.value("viewportLockMode", DEF.viewportLockMode);
  } else {
    applyKeymapPreset();
  }
}

void Editor::Preferences::save()
{
  auto recents = nlohmann::json::array();
  for (const auto &r : recentProjects) {
    recents.push_back({{"path", r.path}, {"name", r.name}, {"cardImage", r.cardImage}});
  }

  std::string json = Utils::JSON::Builder{}
    .set("keymapPreset", (uint32_t)keymapPreset)
    .set("keymap", keymap.serialize(keymapPreset))
    .set("themeName", themeName)
    .set("recentProjects", recents)
    .set("zoomSpeed", zoomSpeed)
    .set("moveSpeed", moveSpeed)
    .set("panSpeed", panSpeed)
    .set("lookSpeed", lookSpeed)
    .set("invertWheelY", invertWheelY)
    .set("renderFactorAA", renderFactorAA)
    .set("useVSync", useVSync)
    .set("fpsLimit", fpsLimit)
    .set("showRotAsEuler", showRotAsEuler)
    .set("mouseWheelModifiesSpeed", mouseWheelModifiesSpeed)
    .set("viewportLockMode", viewportLockMode)
    .toString();
  auto prefPath = getPrefsPath();
  printf("Saving prefs to %s\n", prefPath.c_str());
  Utils::FS::saveTextFile(prefPath, json);
}

void Editor::Preferences::addRecentProject(const std::string &path, const std::string &name, const std::string &cardImage)
{
  constexpr size_t MAX_RECENTS = 12;
  std::erase_if(recentProjects, [&](const RecentProject &r) { return r.path == path; });
  recentProjects.insert(recentProjects.begin(), {path, name, cardImage});
  if (recentProjects.size() > MAX_RECENTS) recentProjects.resize(MAX_RECENTS);
}

void Editor::Preferences::removeRecentProject(const std::string &path)
{
  std::erase_if(recentProjects, [&](const RecentProject &r) { return r.path == path; });
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
