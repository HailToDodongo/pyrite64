/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"

#include <filesystem>
#include <thread>
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "../utils/string.h"
#include "../utils/textureFormats.h"

namespace fs = std::filesystem;

using AT = Project::AssetManager::FileType;

bool Build::buildProject(const std::string &path)
{
  Project::Project project{path};
  Utils::Logger::log("Building project...");

  #if defined(_WIN32)
    _putenv_s("N64_INST", project.conf.pathN64Inst.c_str());
  #else
    setenv("N64_INST", project.conf.pathN64Inst.c_str(), 0);
  #endif

  auto enginePath = fs::current_path() / "n64" / "engine";
  enginePath = fs::absolute(enginePath);

  auto fsDataPath = fs::absolute(fs::path{path} / "filesystem" / "p64");
  if (!fs::exists(fsDataPath)) {
    fs::create_directories(fsDataPath);
  }

  SceneCtx sceneCtx{};
  sceneCtx.project = &project;

  // Global project config
  sceneCtx.files.push_back("filesystem/p64/conf");

  // Asset-Manager
  {
    struct Entry
    {
      std::string path{};
      uint32_t stringOffset{};
      uint32_t type{};
    };

    std::vector<Entry> assetList{};

    uint32_t stringOffset{0};
    uint32_t assetCount = 0;
    for (auto &typed : project.getAssets().getEntries()) {
      for (auto &entry : typed) {
        if (entry.conf.exclude || entry.type == Project::AssetManager::FileType::UNKNOWN) continue;
        sceneCtx.assetUUIDToIdx[entry.uuid] = assetList.size();

        assetList.push_back({
          entry.romPath,
          stringOffset,
          ((uint32_t)entry.type) << 24
        });

        stringOffset += entry.romPath.size() + 1;
        ++assetCount;
      }
    }

    Utils::BinaryFile fileList{};
    fileList.write<uint32_t>(assetCount);
    uint32_t baseOffset = (assetCount * sizeof(uint32_t)*2) + sizeof(uint32_t);
    for (auto &entry : assetList) {
      fileList.write(baseOffset + entry.stringOffset);
      fileList.write(entry.type);
    }
    for (auto &entry : assetList) {
      fileList.writeChars(entry.path.c_str(), entry.path.size()+1);
    }
    fileList.writeToFile(fsDataPath / "a");
  }

  // User scripts
  auto userDirs = Utils::FS::scanDirs(path + "/src/user");
  std::string userCodeRules = "";
  for (const auto &dir : userDirs) {
    userCodeRules += "src += $(wildcard src/user/" + dir + "/*.cpp)\n";
  }

  // Scripts
  buildGlobalScripts(project, sceneCtx);
  buildScripts(project, sceneCtx);

  // Scenes
  project.getScenes().reload();
  const auto &scenes = project.getScenes().getEntries();
  for (const auto &scene : scenes) {
    buildScene(project, scene, sceneCtx);
  }

  // Assets
  if(!buildT3DMAssets(project, sceneCtx)) {
    Utils::Logger::log("T3DM Asset build failed!", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  if(!buildFontAssets(project, sceneCtx)) {
    Utils::Logger::log("Font Asset build failed!", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  if(!buildTextureAssets(project, sceneCtx)) {
    Utils::Logger::log("Texture Asset build failed!", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  if(!buildAudioAssets(project, sceneCtx)) {
    Utils::Logger::log("Audio Asset build failed!", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  // Makefile
  auto makefile = Utils::replaceAll(
    Utils::FS::loadTextFile("data/build/baseMakefile.mk"),
    {
      {"{{N64_INST}}",          project.conf.pathN64Inst},
      {"{{ENGINE_PATH}}",       enginePath},
      {"{{ROM_NAME}}",          project.conf.romName},
      {"{{PROJECT_NAME}}",      project.conf.name},
      {"{{ASSET_LIST}}",        Utils::join(sceneCtx.files, " ")},
      {"{{USER_CODE_DIRS}}",    userCodeRules},
      {"{{P64_SELF_PATH}}",     Utils::Proc::getSelfPath()},
      {"{{PROJECT_SELF_PATH}}", fs::absolute(path).string()},
    }
  );

  auto oldMakefile = Utils::FS::loadTextFile(path + "/Makefile");
  if (oldMakefile != makefile) {
    Utils::Logger::log("Makefile changed, clean build");

    Utils::FS::saveTextFile(path + "/Makefile", makefile);
    Utils::Proc::runSyncLogged("make -C \"" + path + "\" clean");
  }

  {
    Utils::BinaryFile f{};
    f.write<uint32_t>(project.conf.sceneIdOnBoot);
    f.write<uint32_t>(project.conf.sceneIdOnReset);
    for(uint32_t i=0; i<sceneCtx.autoLoadFontUUIDs.size(); ++i) {
      auto uuid = sceneCtx.autoLoadFontUUIDs[i];
      f.write<uint16_t>(uuid == 0 ? 0xFFFF : sceneCtx.assetUUIDToIdx[uuid]);
    }
    f.writeToFile(fsDataPath / "conf");
  }

  // Build
  bool success = Utils::Proc::runSyncLogged("make -C \"" + path + "\" -j8");

  if(success) {
    Utils::Logger::log("Build done!");
  } else {
    Utils::Logger::log("Build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return success;
}


bool Build::assetBuildNeeded(const Project::AssetManager::Entry &asset, const std::string &outPath)
{
  auto ageSrc = Utils::FS::getFileAge(asset.path);
  auto ageDst = Utils::FS::getFileAge(outPath);
  if(ageSrc < ageDst) {
    Utils::Logger::log("Skipping Asset (up to date): " + asset.outPath);
    return false;
  }
  Utils::Logger::log("Building Asset: " + asset.path);
  return true;
}