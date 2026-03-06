/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include <filesystem>

namespace fs = std::filesystem;

bool Build::buildPrefabAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  auto &assets = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::PREFAB);
  for (auto &asset : assets)
  {
    if(asset.conf.exclude)continue;

    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / asset.outPath;
    auto outDir = outPath.parent_path();
    fs::create_directories(outPath.parent_path());

    sceneCtx.files.push_back(Utils::FS::toUnixPath(asset.outPath));

    // @TODO: lazy-build again after refactoring the asset table building
    //if(!assetBuildNeeded(asset, outPath))continue;

    sceneCtx.fileObj = {};
    writeObject(sceneCtx, asset.prefab->obj, true);
    sceneCtx.fileObj.writeToFile(outPath);
    sceneCtx.fileObj = {};
  }

  return true;
}