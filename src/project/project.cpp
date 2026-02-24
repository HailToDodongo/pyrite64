/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "project.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "../utils/fs.h"
#include "../utils/hash.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"
#include "../context.h"
#include "../editor/inputConfig.h"//TODO: move keymap into preferences

namespace
{
  /**
   * Recursively copy changed files from src to dst if file is different.
   * This is used to make sure updated engine code is put in the project.
   * Doing so each time would force a recompile, so the content is checked.
   */
  void copyChangedEngineFiles(const fs::path& src, const fs::path& dst) {

    if (!fs::exists(src)) return;
    if (fs::is_directory(src)) {
      fs::create_directories(dst);
      for (const auto& entry : fs::directory_iterator(src)) {
        copyChangedEngineFiles(entry.path(), dst / entry.path().filename());
      }
    } else {
      std::string srcHash{};
      std::string dstHash{};

      // Read destination file if exists
      if (fs::exists(dst)) {
        std::ifstream ifs(dst, std::ios::binary);
        dstHash = std::string((std::istreambuf_iterator(ifs)), std::istreambuf_iterator<char>());
        {
          std::ifstream ifsSrc(src, std::ios::binary);
          srcHash = std::string((std::istreambuf_iterator(ifsSrc)), std::istreambuf_iterator<char>());
        }
      }

      if (dstHash.empty() || srcHash != dstHash) {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
      }
    }
  }
}

std::string Project::ProjectConf::serialize() const {
  return Utils::JSON::Builder{}
    .set("name", name)
    .set("romName", romName)
    .set("pathEmu", pathEmu)
    .set("pathN64Inst", pathN64Inst)
    .set("sceneIdOnBoot", sceneIdOnBoot)
    .set("sceneIdOnReset", sceneIdOnReset)
    .set("sceneIdLastOpened", sceneIdLastOpened)
    .set("inputPreset", inputPreset)//TODO: move keymap into preferences
    .set("keymap", ctx.inputs.serialize())
    .toString();
}

void Project::Project::deserialize(const nlohmann::json &doc) {
  conf.name = doc.value("name", "New Project");
  conf.romName = doc.value("romName", "pyrite64");
  conf.pathEmu = doc.value("pathEmu", "ares");
  conf.pathN64Inst = doc.value("pathN64Inst", "");
  conf.sceneIdOnBoot = doc.value("sceneIdOnBoot", 1);
  conf.sceneIdOnReset = doc.value("sceneIdOnReset", 1);
  conf.sceneIdLastOpened = doc.value("sceneIdLastOpened", 1);
  conf.inputPreset = doc.value("inputPreset", 0);
  if (doc.contains("keymap")) ctx.inputs.deserialize(doc["keymap"]);
  else ctx.inputs.applyPreset((Editor::InputPreset)conf.inputPreset);
}

Project::Project::Project(const std::string &p64projPath)
  : pathConfig{p64projPath}
{
  path = fs::path(p64projPath).parent_path().string();

  auto configJSON = Utils::JSON::loadFile(pathConfig);
  if (configJSON.empty()) {
    throw std::runtime_error("Not a valid project!");
  }

  // ensure relevant directories and files exist + some basic self-repair
  fs::path f{path};
  fs::create_directories(f);
  fs::create_directories(f / "data");
  fs::create_directories(f / "data" / "scenes");
  fs::create_directories(f / "assets");
  fs::create_directories(f / "assets" / "p64");
  fs::create_directories(f / "src");
  fs::create_directories(f / "src" / "p64");
  fs::create_directories(f / "src" / "user");

  Utils::FS::ensureFile(f / ".gitignore", "data/build/baseGitignore");
  Utils::FS::ensureFile(f / "Makefile.custom", "data/build/baseMakefile.custom");
  Utils::FS::ensureFile(f / "assets" / "p64" / "font.ia4.png", "data/build/assets/font.ia4.png");

  //auto t = SDL_GetTicksNS();
  copyChangedEngineFiles("n64/engine", f / "engine");
  //t = SDL_GetTicksNS() - t;
  //printf("Engine files sync took %.2f ms\n", t / 1e6);

  deserialize(configJSON);
  savedState = conf.serialize();
  assets.reload();
  scenes.reload();
}

void Project::Project::saveConfig()
{
  auto serializedConfig = conf.serialize();
  Utils::FS::saveTextFile(pathConfig, serializedConfig);
  savedState = serializedConfig;
}

void Project::Project::save() {
  saveConfig();
  assets.save();
  scenes.save();
  markSaved();
}

