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

bool Build::buildFontAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path mkFont = fs::path{project.conf.pathN64Inst} / "bin" / "mkfont";
  auto &fonts = sceneCtx.project->getAssets().getTypeEntries(Project::AssetManager::FileType::FONT);
  for (auto &font : fonts)
  {
    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / font.outPath;
    auto outDir = outPath.parent_path();

    sceneCtx.files.push_back(font.outPath);

    if(!assetBuildNeeded(font, outPath))continue;

    int compr = (int)font.conf.compression - 1;
    if(compr < 0)compr = 1; // @TODO: pull default compression level

    std::string cmd = mkFont.string() + " -c " + std::to_string(compr);
    cmd += " -o " + outDir.string();
    cmd += " -s " + std::to_string(font.conf.baseScale);
    cmd += " " + font.path;

    if(!Utils::Proc::runSyncLogged(cmd)) {
      return false;
    }
  }
  return true;
}