/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include "../utils/fs.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iterator>

#include "../project/graph/graph.h"

namespace fs = std::filesystem;

namespace
{
  // Whether the file at 'path' already holds exactly 'bytes'.
  bool fileMatches(const fs::path &path, const std::vector<uint8_t> &bytes)
  {
    std::error_code ec{};
    if(!fs::exists(path) || fs::file_size(path, ec) != bytes.size())return false;
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> cur((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return cur.size() == bytes.size() && std::equal(cur.begin(), cur.end(), bytes.begin());
  }
}

bool Build::buildNodeGraphAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path sourcePath = fs::path{project.getPath()} / "src" / "p64";
  auto &assets = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::NODE_GRAPH);
  for (auto &asset : assets)
  {
    if(asset.conf.exclude)continue;

    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / asset.outPath;
    fs::create_directories(outPath.parent_path());

    std::string sourceName = Utils::toHex64(asset.getUUID()) + ".cpp";
    fs::path sourceOutPath = sourcePath / sourceName;

    sceneCtx.files.push_back(Utils::FS::toUnixPath(asset.outPath));
    sceneCtx.graphFunctions.push_back(asset.getUUID());

    // Always regenerate: output also depends on node definitions + codegen, not just the asset.
    auto json = Utils::FS::loadTextFile(asset.path);
    Project::Graph::Graph graph{};
    graph.deserialize(json);

    Utils::BinaryFile binFile{};
    std::string sourceCode{};
    sourceCode += "// AUTO-GENERATED FILE\n";
    sourceCode += "// File: " + asset.getName() + "\n\n";
    graph.build(binFile, sourceCode, asset.getUUID());

    if(!fs::exists(sourceOutPath) || Utils::FS::loadTextFile(sourceOutPath) != sourceCode) {
      Utils::FS::saveTextFile(sourceOutPath, sourceCode);
    }
    if(!fileMatches(outPath, binFile.getData())) {
      binFile.writeToFile(outPath);
    }
  }
  return true;
}
