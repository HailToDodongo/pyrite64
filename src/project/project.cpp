/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "project.h"

#include <filesystem>

#include "../utils/fs.h"

#include "../utils/json.h"
#include "../utils/jsonBuilder.h"

std::string Project::ProjectConf::serialize() const {
  Utils::JSON::Builder builder{};
  builder.set("name", name);
  builder.set("romName", romName);
  builder.set("pathEmu", pathEmu);
  builder.set("pathN64Inst", pathN64Inst);
  builder.set("sceneIdOnBoot", sceneIdOnBoot);
  builder.set("sceneIdOnReset", sceneIdOnReset);
  return builder.toString();
}

void Project::Project::deserialize(const nlohmann::json &doc) {
  conf.name = doc.value("name", "New Project");
  conf.romName = doc.value("romName", "pyrite64");
  conf.pathEmu = doc.value("pathEmu", "ares");
  conf.pathN64Inst = doc.value("pathN64Inst", "");
  conf.sceneIdOnBoot = doc.value("sceneIdOnBoot", 1);
  conf.sceneIdOnReset = doc.value("sceneIdOnReset", 1);
}

Project::Project::Project(const std::string &path)
  : path{path}, pathConfig{path + "/project.json"}
{
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

  // Copy engine directory into project.
  // This avoids issues if the editor itself is sitting in a directory with spaces (e.g. "C:/Program Files/...").
  // @TODO: also update the engine on changes by keeping some hash file
  if(!fs::exists(f / "engine")) {
    fs::create_directories(f / "engine");
    // do individual copies to avoid build-files
    auto opt = fs::copy_options::recursive | fs::copy_options::overwrite_existing;
    fs::copy_file("n64/engine/Makefile",   f / "engine" / "Makefile", opt);
    fs::copy_file("n64/engine/.gitignore", f / "engine" / ".gitignore", opt);
    fs::copy(     "n64/engine/src",        f / "engine" / "src", opt);
    fs::copy(     "n64/engine/include",    f / "engine" / "include", opt);
  }

  deserialize(configJSON);
  assets.reload();
  scenes.reload();
}

void Project::Project::save() {
  Utils::FS::saveTextFile(pathConfig, conf.serialize());
  assets.save();
  scenes.save();
}
