/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include <filesystem>

#include "../utils/binaryFile.h"
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "tiny3d/tools/gltf_importer/src/parser.h"

namespace fs = std::filesystem;

namespace Build {
  std::vector<uint8_t> buildCollision(const std::string &gltfPath, float baseScale);
}

bool Build::buildT3DMAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path mkAsset = fs::path{project.conf.pathN64Inst} / "bin" / "mkasset";
  auto &models = sceneCtx.project->getAssets().getTypeEntries(Project::AssetManager::FileType::MODEL_3D);
  for (auto &model : models)
  {
    auto projectPath = fs::path{project.getPath()};
    auto t3dmPath = projectPath / model.outPath;
    auto t3dmDir = t3dmPath.parent_path();

    sceneCtx.files.push_back(model.outPath);

    if(!assetBuildNeeded(model, t3dmPath))continue;
    Utils::FS::ensureDir(t3dmDir);

    T3DM::config = {
      .globalScale = (float)model.conf.baseScale,
      .animSampleRate = 60,
      //.ignoreMaterials = args.checkArg("--ignore-materials"),
      //.ignoreTransforms = args.checkArg("--ignore-transforms"),
      .createBVH = model.conf.gltfBVH,
      .verbose = false,
      .assetPath = "assets/",
      .assetPathFull = fs::absolute(project.getPath() + "/assets").string(),
    };

    auto t3dm = T3DM::parseGLTF(model.path.c_str());

    std::vector<T3DM::CustomChunk> customChunks{};

    if(model.conf.gltfCollision.value) {
      customChunks.emplace_back('0', buildCollision(model.path, T3DM::config.globalScale));
    }

    T3DM::writeT3DM(t3dm, t3dmPath.c_str(), projectPath, customChunks);

    int compr = (int)model.conf.compression - 1;
    if(compr < 0)compr = 1; // @TODO: pull default compression level

    std::string cmd = mkAsset.string() + " -c " + std::to_string(compr);
    cmd += " -o " + t3dmDir.string();
    cmd += " " + t3dmPath.string();

    if(!Utils::Proc::runSyncLogged(cmd)) {
      return false;
    }
  }
  return true;
}