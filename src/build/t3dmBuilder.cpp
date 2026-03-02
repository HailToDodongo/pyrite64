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

bool Build::buildT3DCollision(
  Project::Project &project, SceneCtx &sceneCtx,
  const std::unordered_set<std::string> &meshes,
  uint64_t orgUUID,
  uint64_t newUUID
)
{
  auto model = project.getAssets().getEntryByUUID(orgUUID);
  if(!model) {
    Utils::Logger::log("T3DM Collision Build: Model not found!", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  auto fileName = Utils::toHex64(newUUID);
  auto projectPath = fs::path{project.getPath()};
  auto outPath = projectPath / "filesystem" / fileName;

  Project::AssetManagerEntry entry{
    .name = model->name,
    .path = model->path,
    .outPath = "filesystem/" + fileName,
    .romPath = "rom:/" + fileName,
    .type = Project::FileType::UNKNOWN,
  };
  entry.conf.uuid = newUUID;

  printf("Building T3DM Collision: %s\n", outPath.string().c_str());
  //printf(" asset: %d | %d\n", sceneCtx.files.size(), sceneCtx.assetUUIDToIdx.size());

  auto collData = Build::buildCollision(model->path, model->conf.baseScale, meshes);
  collData.writeToFile(outPath.string());

  fs::path mkAsset = fs::path{project.conf.pathN64Inst} / "bin" / "mkasset";
  std::string cmd = mkAsset.string() + " -c 1";;
  cmd += " -o \"" + outPath.parent_path().string() + "\"";
  cmd += " \"" + outPath.string() + "\"";

  if(!sceneCtx.toolchain.runCmdSyncLogged(cmd)) {
    return false;
  }

  sceneCtx.addAsset(entry);

  return true;
}

bool Build::buildT3DMAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path mkAsset = fs::path{project.conf.pathN64Inst} / "bin" / "mkasset";
  auto &models = sceneCtx.project->getAssets().getTypeEntries(Project::FileType::MODEL_3D);
  auto projectPath = fs::path{project.getPath()};

  for (auto &model : models)
  {
    auto t3dmPath = projectPath / model.outPath;
    auto t3dmDir = t3dmPath.parent_path();

    sceneCtx.files.push_back(Utils::FS::toUnixPath(model.outPath));

    if(assetBuildNeeded(model, t3dmPath)) {
      fs::create_directories(t3dmDir);

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

      T3DM::writeT3DM(t3dm, t3dmPath.string().c_str(), projectPath, customChunks);

      int compr = (int)model.conf.compression - 1;
      if(compr < 0)compr = 1; // @TODO: pull default compression level

      std::string cmd = mkAsset.string() + " -c " + std::to_string(compr);
      cmd += " -o \"" + t3dmDir.string() + "\"";
      cmd += " \"" + t3dmPath.string() + "\"";

      if(!sceneCtx.toolchain.runCmdSyncLogged(cmd)) {
        return false;
      }
    }

    // search for all files containing *.sdata
    for (const auto &entry : fs::directory_iterator{t3dmDir}) {
      if (entry.is_regular_file()) {
        auto path = entry.path();
        auto name = entry.path().filename();

        if (path.extension() == ".sdata") {
          auto fileName = t3dmPath.stem().string();
          if (name.string().starts_with(fileName)) {
            // path relative to project
            auto relPath = fs::relative(path, projectPath).string();
            sceneCtx.files.push_back(Utils::FS::toUnixPath(relPath));
          }
        }
      }
    }
  }
  return true;
}