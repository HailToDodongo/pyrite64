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
  auto &fonts = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::FONT);
  for (auto &font : fonts)
  {
    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / font.outPath;
    auto outDir = outPath.parent_path();
    fs::create_directories(outPath.parent_path());

    sceneCtx.files.push_back(Utils::FS::toUnixPath(font.outPath));

    uint32_t fontId = font.conf.fontId.value;
    if(fontId > 0 && fontId < sceneCtx.autoLoadFontUUIDs.size()) {
      sceneCtx.autoLoadFontUUIDs[fontId] = font.getUUID();
    }

    if(!assetBuildNeeded(font, outPath))continue;

    int compr = (int)font.conf.compression - 1;
    if(compr < 0)compr = 1; // @TODO: pull default compression level

    fs::path charsetFile{};
    if(!font.conf.fontCharset.value.empty()) {
      charsetFile = outDir / (font.name + "_charset.txt");
      Utils::FS::saveTextFile(charsetFile, font.conf.fontCharset.value);
    }

    std::string cmd = mkFont.string() + " -c " + std::to_string(compr);
    cmd += " -o \"" + outDir.string() + "\"";
    cmd += " -s " + std::to_string(font.conf.baseScale);
    if(!charsetFile.empty())cmd += " --charset \"" + charsetFile.string() + "\"";
    cmd += " \"" + font.path + "\"";

    bool res = sceneCtx.toolchain.runCmdSyncLogged(cmd);
    fs::remove(charsetFile);
    if(!res)return false;
  }
  return true;
}
