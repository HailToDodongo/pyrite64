/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include <filesystem>

#include "../utils/textureFormats.h"
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "tools/bci.h"

namespace fs = std::filesystem;

bool Build::buildTextureAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path mkAsset = fs::path{project.conf.pathN64Inst} / "bin" / "mkasset";
  fs::path mkSprite = fs::path{project.conf.pathN64Inst} / "bin" / "mksprite";

  auto &images = sceneCtx.project->getAssets().getTypeEntries(Project::AssetManager::FileType::IMAGE);
  for (auto &image : images)
  {
    if (image.conf.exclude)continue;

    auto outPath = image.outPath;
    if(image.conf.format == (int)Utils::TexFormat::BCI_256) {
      // replace .sprite with .bci
      outPath = outPath.substr(0, outPath.size() - 7) + ".bci";
    }

    sceneCtx.files.push_back(outPath);

    auto assetPath = fs::path{project.getPath()} / outPath;
    auto assetDir = assetPath.parent_path();

    if(!assetBuildNeeded(image, assetPath.string()))continue;

    int compr = (int)image.conf.compression - 1;
    if(compr < 0)compr = 1; // @TODO: pull default compression level

    if(image.conf.format == (int)Utils::TexFormat::BCI_256)
    {
      BCI::convertPNG(image.path, assetPath.string());
    } else {
      std::string cmd = mkSprite.string() + " -c " + std::to_string(compr);
      if (image.conf.format != 0) {
        cmd += std::string{" -f "} + Utils::TEX_TYPES[image.conf.format];
      }
      cmd += " -o " + assetDir.string();
      cmd += " " + image.path;

      if(!Utils::Proc::runSyncLogged(cmd)) {
        return false;
      }
    }
  }
  return true;
}