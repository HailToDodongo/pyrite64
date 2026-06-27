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

    // A prefab asset is spawned at runtime, which has no prefab resolution or transform
    // hierarchy. So bake the whole tree flat and world-relative to the root, expanding any
    // nested prefabs, and prefix the object count so the runtime knows how many to load.
    sceneCtx.fileObj = {};
    sceneCtx.nextRuntimeId = 1; // the root is 0, expanded children continue from here
    sceneCtx.fileObj.write<uint32_t>(0); // object count, patched below
    uint32_t count = writeObject(sceneCtx, asset.prefab->obj, false, 0, 0, true, {}, true);
    sceneCtx.fileObj.atPos(0, [&]{ sceneCtx.fileObj.write<uint32_t>(count); });
    sceneCtx.fileObj.writeToFile(outPath);
    sceneCtx.fileObj = {};
  }

  return true;
}