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

bool Build::buildAudioAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path mkAudio = fs::path{project.conf.pathN64Inst} / "bin" / "audioconv64";
  auto &assets = sceneCtx.project->getAssets().getTypeEntries(Project::AssetManager::FileType::AUDIO);
  for (auto &asset : assets)
  {
    if(asset.conf.exclude)continue;

    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / asset.outPath;
    auto outDir = outPath.parent_path();
    Utils::FS::ensureDir(outPath.parent_path());

    sceneCtx.files.push_back(asset.outPath);

    if(!assetBuildNeeded(asset, outPath))continue;

    std::string cmd = mkAudio.string();
    if(asset.conf.wavForceMono.value) {
      cmd += " --wav-mono";
    }
    if(asset.conf.wavResampleRate.value != 0) {
      cmd += " --wav-resample " + std::to_string(asset.conf.wavResampleRate.value);
    }

    cmd += " --wav-compress " + std::to_string(asset.conf.wavCompression.value);
    cmd += " -o " + outDir.string();
    cmd += " " + asset.path;

    if(!Utils::Proc::runSyncLogged(cmd)) {
      return false;
    }
  }
  return true;
}