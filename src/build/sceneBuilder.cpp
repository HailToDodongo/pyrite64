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

#include "engine/include/scene/objectFlags.h"

namespace T3D
{
 #include "tiny3d/tools/gltf_importer/src/math/quantizer.h"
}

namespace fs = std::filesystem;

namespace
{
  constexpr uint32_t FLAG_CLR_DEPTH = 1 << 0;
  constexpr uint32_t FLAG_CLR_COLOR = 1 << 1;
  constexpr uint32_t FLAG_SCR_32BIT = 1 << 2;
}

void Build::writeObject(Build::SceneCtx &ctx, Project::Object &obj, bool savePrefabItself)
{
  auto srcObj = &obj;
  if(!savePrefabItself && obj.isPrefabInstance())
  {
    auto prefab = ctx.project->getAssets().getPrefabByUUID(srcObj->uuidPrefab.value);
    if(prefab)srcObj = &prefab->obj;
  }

  uint16_t objFlags = 0;
  if(obj.enabled)objFlags |= P64::ObjectFlags::ACTIVE;
  if(!obj.children.empty())objFlags |= P64::ObjectFlags::IS_GROUP;

  ctx.fileObj.write<uint16_t>(objFlags); // @TODO type
  ctx.fileObj.write<uint16_t>(obj.id);
  ctx.fileObj.write<uint16_t>(obj.parent ? obj.parent->id : 0);
  ctx.fileObj.write<uint16_t>(0); // padding
  ctx.fileObj.write(srcObj->pos.resolve(obj.propOverrides));
  ctx.fileObj.write(srcObj->scale.resolve(obj.propOverrides));

  auto &rot = srcObj->rot.resolve(obj.propOverrides);
  uint32_t quatQuant = T3D::Quantizer::quatTo32Bit({rot.x, rot.y, rot.z, rot.w});
  ctx.fileObj.write(quatQuant);

  // DATA
  for (auto &comp : srcObj->components) {
    auto compPos = ctx.fileObj.getPos();
    ctx.fileObj.skip(2);
    ctx.fileObj.skip(2); // flags (@TODO)

    if (comp.id >= 0 && comp.id < Project::Component::TABLE.size()) {
      Project::Component::TABLE[comp.id].funcBuild(obj, comp, ctx);
    } else {
      Utils::Logger::log("Component ID not found: " + std::to_string(comp.id), Utils::Logger::LEVEL_ERROR);
      assert(false);
    }

    ctx.fileObj.align(4);
    auto size = (ctx.fileObj.getPos() - compPos) / 4;
    assert(size < 256);

    ctx.fileObj.posPush(compPos);
    ctx.fileObj.write<uint8_t>(comp.id);
    ctx.fileObj.write<uint8_t>(size);
    ctx.fileObj.posPop();
    //ctx.fileObj.write<uint16_t>(comp.id);
  }

  ctx.fileObj.write<uint32_t>(0);

  for (const auto &child : obj.children) {
    writeObject(ctx, *child, savePrefabItself);
  }
}

void Build::buildScene(Project::Project &project, const Project::SceneEntry &scene, SceneCtx &ctx)
{
  std::string fileNameScene = "s" + Utils::padLeft(std::to_string(scene.id), '0', 4);
  std::string fileNameObj = fileNameScene + "o";

  std::unique_ptr<Project::Scene> sc{new Project::Scene(scene.id, project.getPath())};

  auto fsDataPath = fs::absolute(fs::path{project.getPath()} / "filesystem" / "p64");

  uint32_t sceneFlags = 0;
  uint32_t objCount = sc->objectsMap.size();

  if (sc->conf.doClearDepth.value)sceneFlags |= FLAG_CLR_DEPTH;
  if (sc->conf.doClearColor.value)sceneFlags |= FLAG_CLR_COLOR;
  if (sc->conf.fbFormat)sceneFlags |= FLAG_SCR_32BIT;

  ctx.fileObj = {};
  auto &rootObj = sc->getRootObject();
  for (const auto &child : rootObj.children) {
    writeObject(ctx, *child, false);
  }

  ctx.fileObj.writeToFile(fsDataPath / fileNameObj);

  ctx.fileScene = {};
  ctx.fileScene.write<uint16_t>(sc->conf.fbWidth);
  ctx.fileScene.write<uint16_t>(sc->conf.fbHeight);
  ctx.fileScene.write(sceneFlags);
  ctx.fileScene.writeRGBA(sc->conf.clearColor);
  ctx.fileScene.write(objCount);

  ctx.fileScene.write<uint8_t>(sc->conf.renderPipeline.value);
  ctx.fileScene.write<uint8_t>(0); // padding
  ctx.fileScene.write<uint8_t>(0); // padding
  ctx.fileScene.write<uint8_t>(0); // padding

  // Layer::Setup
  ctx.fileScene.write<uint8_t>(sc->conf.layers3D.size());
  ctx.fileScene.write<uint8_t>(sc->conf.layersPtx.size());
  ctx.fileScene.write<uint8_t>(sc->conf.layers2D.size());
  ctx.fileScene.write<uint8_t>(0); // padding

  auto writeLayer = [&ctx](const Project::LayerConf &layer) {
    uint32_t flags = 0;
    if(layer.depthWrite.value)flags |= (1 << 0);
    if(layer.depthCompare.value)flags |= (1 << 1);

    ctx.fileScene.write<uint32_t>(flags);
    ctx.fileScene.write<uint32_t>(layer.blender.value);
  };

  for(const auto &layer : sc->conf.layers3D)writeLayer(layer);
  for(const auto &layer : sc->conf.layersPtx)writeLayer(layer);
  for(const auto &layer : sc->conf.layers2D)writeLayer(layer);

  ctx.fileScene.align(4);
  ctx.fileScene.writeToFile(fsDataPath / fileNameScene);

  ctx.files.push_back("filesystem/p64/" + fileNameScene);
  ctx.files.push_back("filesystem/p64/" + fileNameObj);
}
