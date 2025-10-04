/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"

#include <filesystem>
#include "../utils/fs.h"
#include "../utils/proc.h"
#include "../utils/string.h"
#include "../utils/textureFormats.h"

namespace fs = std::filesystem;

using AT = Project::AssetManager::FileType;

namespace
{
  std::string getAssetROMPath(const std::string &path, const std::string &basePath)
  {
    auto pathAbs = fs::absolute(path).string();
    pathAbs = pathAbs.substr(basePath.length());
    pathAbs = Utils::replaceFirst(pathAbs, "/assets/", "filesystem/");
    return pathAbs;
  }

  std::string changeExt(const std::string &path, const std::string &newExt)
  {
    auto p = fs::path(path);
    p.replace_extension(newExt);
    return p.string();
  }

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
    for (const auto &entry : entries)
    {
      auto comprLevel = std::to_string(getComprLevel(entry.conf.compression));
      auto romPath = getAssetROMPath(entry.path, projectBase);

      switch(entry.type)
      {
        case AT::IMAGE:
          assetList.push_back(changeExt(romPath, ".sprite"));
          rules += assetList.back() + ": MKSPRITE_FLAGS = -c " + comprLevel;
          if (entry.conf.format != 0) {
            rules += std::string{" -f "} + Utils::TEX_TYPES[entry.conf.format];
          }
          rules += '\n';
        break;

        case AT::MODEL_3D:
          assetList.push_back(changeExt(romPath, ".t3dm"));
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
          assetList.push_back(changeExt(romPath, ".wav64"));
          // @TODO: handle XM
        break;

        default: break;
      }
    }

    return rules;
  }
}

bool Build::buildProject(Project::Project &project) {

  auto path = project.getPath();
  printf("Building project (%s)...\n", path.c_str());

  auto enginePath = fs::current_path() / "n64" / "engine";
  enginePath = fs::absolute(enginePath);

  auto fsDataPath = fs::absolute(fs::path{path} / "filesystem" / "p64");
  if (!fs::exists(fsDataPath)) {
    fs::create_directories(fsDataPath);
  }

  SceneCtx sceneCtx{};

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
  makefile = Utils::replaceAll(makefile, "{{RULES_ASSETS}}", genAssetRules(project, sceneCtx.files));
  makefile = Utils::replaceAll(makefile, "{{ASSET_LIST}}", Utils::join(sceneCtx.files, " "));

  auto oldMakefile = Utils::FS::loadTextFile(path + "/Makefile");
  if (oldMakefile != makefile) {
    printf("Makefile changed, clean build\n");

    Utils::FS::saveTextFile(path + "/Makefile", makefile);
    auto res = Utils::Proc::runSync("make -C \"" + path + "\" clean");
    printf("Make-Clean: %s\n", res.c_str());
  }

  // Build
  auto res = Utils::Proc::runSync("make -C \"" + path + "\" -j8");
  printf("Make: %s\n", res.c_str());

  return true;
}
