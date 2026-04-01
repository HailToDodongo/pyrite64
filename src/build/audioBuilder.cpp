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

  auto procAsset = [&](const Project::AssetManagerEntry &asset)
  {
    if(asset.conf.exclude)return true;

    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / asset.outPath;
    auto outDir = outPath.parent_path();
    fs::create_directories(outPath.parent_path());

    sceneCtx.files.push_back(Utils::FS::toUnixPath(asset.outPath));

    if(!assetBuildNeeded(asset, outPath))return true;

    std::string cmd = mkAudio.string();

    if(asset.type == Project::FileType::AUDIO)
    {
      if(asset.conf.wavForceMono.value) {
        cmd += " --wav-mono";
      }
      if(asset.conf.wavResampleRate.value != 0) {
        cmd += " --wav-resample " + std::to_string(asset.conf.wavResampleRate.value);
      }

      cmd += " --wav-compress " + std::to_string(asset.conf.wavCompression.value);
    }

    cmd += " -o \"" + outDir.string() + "\"";
    cmd += " \"" + asset.path + "\"";

    if(!sceneCtx.toolchain.runCmdSyncLogged(cmd)) {
      return false;
    }
    return true;
  };

  auto &assetsAudio = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::AUDIO);
  auto &assetsMusic = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::MUSIC_XM);

  for (auto &asset : assetsAudio) {
    if(!procAsset(asset))return false;
  }
  for (auto &asset : assetsMusic) {
    if(!procAsset(asset))return false;
  }
  return true;
}