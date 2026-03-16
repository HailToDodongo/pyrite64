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
#include "../../n64/engine/include/renderer/material.h"

namespace fs = std::filesystem;

namespace
{
  bool matWriter(
    Build::SceneCtx &sceneCtx,
    std::shared_ptr<BinaryFile> f,
    const Project::Assets::Material &mat
  ) {
    uint32_t flags = 0;

    auto posStart = f->getPos();
    f->write<uint32_t>(0); // set later
    f->write<uint32_t>(mat.drawFlags.value);

    if(mat.ccSet.value) {
      flags |= P64::Renderer::Material::FLAG_CC;
      f->write(mat.cc.value);
    }
    if(mat.blenderSet.value) {
      flags |= P64::Renderer::Material::FLAG_BLENDER;
      f->write(mat.blender.value);
    }
    if(mat.fogSet.value) {
      flags |= P64::Renderer::Material::FLAG_FOG;
      f->write(mat.fog.value);
    }
    if(mat.primColorSet.value) {
      flags |= P64::Renderer::Material::FLAG_PRIM;
      auto col = mat.primColor.value * 255.0f;
      f->write((uint8_t)col.r);
      f->write((uint8_t)col.g);
      f->write((uint8_t)col.b);
      f->write((uint8_t)col.a);
    }
    if(mat.envColorSet.value) {
      flags |= P64::Renderer::Material::FLAG_ENV;
      auto col = mat.envColor.value * 255.0f;
      f->write((uint8_t)col.r);
      f->write((uint8_t)col.g);
      f->write((uint8_t)col.b);
      f->write((uint8_t)col.a);
    }

    f->posPush();
      f->setPos(posStart);
      f->write(flags);
    f->posPop();

    //std::vector materials{&material.texA, &material.texB};
    //for(const T3DM::MaterialTexture* mat_ : materials)
/*    {
      const T3DM::MaterialTexture&mat;// = *mat_;

      f->write((uint32_t)0); // runtime pointer

      if(!mat.texPathRom.empty()) {
        auto asset = sceneCtx.project->getAssets().getByPath(mat.texPath);
        uint32_t assetIdx = sceneCtx.assetUUIDToIdx[asset->getUUID()];
        f->write<uint16_t>(0);
        f->write<uint16_t>(assetIdx);
      } else {
        f->write<uint16_t>(mat.texReference);
        f->write<uint16_t>(0xFFFF);
      }

      f->write((uint16_t)mat.texWidth);
      f->write((uint16_t)mat.texHeight);

      auto writeTile = [&](const T3DM::TileParam &tile) {
        f->write(tile.low);
        f->write(tile.high);
        f->write(tile.mask);
        f->write(tile.shift);
        f->write(tile.mirror);
        f->write(tile.clamp);
      };
      writeTile(mat.s);
      writeTile(mat.t);
    }
    */
    return true;
  }
}

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

    if(assetBuildNeeded(model, t3dmPath))
    {
      fs::create_directories(t3dmDir);

      T3DM::Config config{
        .globalScale = (float)model.conf.baseScale,
        .createBVH = model.conf.gltfBVH,
        .verbose = false,
        .assetPath = "assets/",
        .assetPathFull = fs::absolute(project.getPath() + "/assets").string(),
        .projectPath = projectPath,
      };

      auto &t3dm = model.model.t3dm;

      config.materialWriter = [&sceneCtx, &model](std::shared_ptr<BinaryFile> f, const T3DM::Material &material, uint32_t matIdx) {
        auto pyriteMat = model.model.materials.find(material.name);
        if(pyriteMat != model.model.materials.end()) {
          printf("Using custom material writer for '%s'\n", material.name.c_str());
          return matWriter(sceneCtx, f, pyriteMat->second);
        }
        throw std::runtime_error("Missing material: " + material.name);
      };

      T3DM::writeT3DM(config, t3dm, t3dmPath.string().c_str());

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