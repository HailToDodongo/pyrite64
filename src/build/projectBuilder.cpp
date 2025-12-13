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

namespace
{
  std::string genAssetRules(Project::Project &project, std::vector<std::string> &assetList)
  {
    std::string rules = "";
    auto entries = project.getAssets().getEntries();
    auto projectBase = fs::absolute(project.getPath()).string();

    auto getComprLevel = [](Project::ComprTypes type) -> int {
      int level = (int)type - 1;
      if(level < 0) {
        level = 1; // @TODO: add project default
      }
      return level;
    };

    std::string rulePath{};
    for (const auto &typed : entries) {
      for (const auto &entry : typed)
      {
        if (entry.conf.exclude)continue;
        auto comprLevel = std::to_string(getComprLevel(entry.conf.compression));

        switch(entry.type)
        {
          case AT::IMAGE:
            assetList.push_back(entry.outPath);
            rules += assetList.back() + ": MKSPRITE_FLAGS = -c " + comprLevel;
            if (entry.conf.format != 0) {
              rules += std::string{" -f "} + Utils::TEX_TYPES[entry.conf.format];
            }
            rules += '\n';
            break;

          case AT::MODEL_3D:
            assetList.push_back(entry.outPath);
            rules += assetList.back() + ": T3DM_ASSET_FLAGS = -c " + comprLevel + "\n";
            rules += assetList.back() + ": T3DM_FLAGS = ";
            if (entry.conf.baseScale != 0) {
              rules += std::string{" --base-scale="} + std::to_string(entry.conf.baseScale);
            }
            if (entry.conf.gltfBVH) {
              rules += std::string{" --bvh"};
            }
            rules += '\n';
            break;

          case AT::AUDIO:
            assetList.push_back(entry.outPath);
            // @TODO: handle XM
            break;

          default: break;
        }
      }
    }

    return rules;
  }
}

bool Build::buildProject(std::string path)
{
  Project::Project project{path};
  Utils::Logger::log("Building project...");

  auto enginePath = fs::current_path() / "n64" / "engine";
  enginePath = fs::absolute(enginePath);

  auto fsDataPath = fs::absolute(fs::path{path} / "filesystem" / "p64");
  if (!fs::exists(fsDataPath)) {
    fs::create_directories(fsDataPath);
  }

  SceneCtx sceneCtx{};
  sceneCtx.project = &project;

  auto mkAssetRules = genAssetRules(project, sceneCtx.files);

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

  // Makefile
  auto makefile = Utils::FS::loadTextFile("data/build/baseMakefile.mk");

  makefile = Utils::replaceAll(makefile, "{{N64_INST}}", project.conf.pathN64Inst);
  makefile = Utils::replaceAll(makefile, "{{ENGINE_PATH}}", enginePath);
  makefile = Utils::replaceAll(makefile, "{{ROM_NAME}}", project.conf.romName);
  makefile = Utils::replaceAll(makefile, "{{PROJECT_NAME}}", project.conf.name);
  makefile = Utils::replaceAll(makefile, "{{RULES_ASSETS}}", mkAssetRules);
  makefile = Utils::replaceAll(makefile, "{{ASSET_LIST}}", Utils::join(sceneCtx.files, " "));
  makefile = Utils::replaceAll(makefile, "{{USER_CODE_DIRS}}", userCodeRules);
  makefile = Utils::replaceAll(makefile, "{{P64_SELF_PATH}}", Utils::Proc::getSelfPath());
  makefile = Utils::replaceAll(makefile, "{{PROJECT_SELF_PATH}}", fs::absolute(path).string());

  auto oldMakefile = Utils::FS::loadTextFile(path + "/Makefile");
  if (oldMakefile != makefile) {
    Utils::Logger::log("Makefile changed, clean build");

    Utils::FS::saveTextFile(path + "/Makefile", makefile);
    Utils::Proc::runSyncLogged("make -C \"" + path + "\" clean");
  }

  // Build
  bool res = Utils::Proc::runSyncLogged("make -C \"" + path + "\" -j8");

  if(res) {
    Utils::Logger::log("Build done!");
  } else {
    Utils::Logger::log("Build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return res;
}
