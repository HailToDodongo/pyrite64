/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene.h"
#include "object.h"
#include <SDL3/SDL.h>
#include "simdjson.h"
#include "../../utils/json.h"
#include "../../context.h"
#include "../../utils/hash.h"
#include "../../utils/jsonBuilder.h"
#include "../../utils/logger.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
  constinit uint64_t nextUUID{1};
  constexpr float DEF_MODEL_SCALE = 1.0f;
}

std::string Project::SceneConf::serialize() const {

  auto writeLayer = [](Utils::JSON::Builder &b, const LayerConf &layer) {
    b.set(layer.name);
    b.set(layer.depthCompare);
    b.set(layer.depthWrite);
    b.set(layer.blender);
  };

  Utils::JSON::Builder builder{};
  builder.set(name)
    .set("fbWidth", fbWidth)
    .set("fbHeight", fbHeight)
    .set("fbFormat", fbFormat)
    .set("clearColor", clearColor)
    .set(doClearColor)
    .set(doClearDepth)
    .set(renderPipeline)
    .setArray<LayerConf>("layers3D", layers3D, writeLayer)
    .setArray<LayerConf>("layersPtx", layersPtx, writeLayer)
    .setArray<LayerConf>("layers2D", layers2D, writeLayer);

  return builder.toString();
}

Project::Scene::Scene(int id_, const std::string &projectPath)
  : id{id_}
{
  Utils::Logger::log("Loading scene: " + std::to_string(id));
  scenePath = projectPath + "/data/scenes/" + std::to_string(id);

  deserialize(Utils::FS::loadTextFile(scenePath + "/scene.json"));

  nextUUID = 1;
  for (const auto& [uuid, obj] : objectsMap) {
    if (obj->id >= nextUUID) {
      nextUUID = obj->id + 1;
    }
  }

  root.id = 0;
  root.name = "Scene";
  root.uuid = Utils::Hash::sha256_64bit(root.name);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(std::string &objJson)
{
  auto obj = std::make_shared<Object>(root);
  obj->deserialize(this, Utils::JSON::load(objJson));
  obj->id = nextUUID++;
  obj->name += " ("+std::to_string(obj->id)+")";
  obj->uuid = Utils::Hash::sha256_64bit(obj->name + std::to_string(rand()));
  obj->pos.value += glm::vec3{10.0f, 0.0f, 0.0f};
  return addObject(root, obj);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object &parent) {
  auto child = std::make_shared<Object>(parent);
  child->id = nextUUID++;
  child->name = "New Object ("+std::to_string(child->id)+")";
  child->uuid = Utils::Hash::sha256_64bit(child->name + std::to_string(rand()));
  child->scale.value = {DEF_MODEL_SCALE, DEF_MODEL_SCALE, DEF_MODEL_SCALE};
  child->rot.value = glm::identity<glm::quat>();
  return addObject(parent, child);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object&parent, std::shared_ptr<Object> obj) {
  parent.children.push_back(obj);
  objectsMap[obj->uuid] = obj;
  return obj;
}

std::shared_ptr<Project::Object> Project::Scene::addPrefabInstance(uint64_t prefabUUID)
{
  auto prefab = ctx.project->getAssets().getPrefabByUUID(prefabUUID);
  if (!prefab)return nullptr;

  auto obj = std::make_shared<Object>(root);
  obj->id = nextUUID++;
  obj->name += prefab->obj.name + " ("+std::to_string(obj->id)+")";
  obj->uuid = Utils::Hash::randomU32();
  obj->pos = prefab->obj.pos;
  obj->rot = prefab->obj.rot;
  obj->scale = prefab->obj.scale;

  obj->uuidPrefab.value = prefab->uuid.value; // Link to prefab
  return addObject(root, obj);
}

void Project::Scene::removeObject(Object &obj) {
  if (ctx.selObjectUUID == obj.uuid) {
    ctx.selObjectUUID = 0;
  }

  std::erase_if(
    obj.parent->children,
    [&obj](const std::shared_ptr<Object> &ref) { return ref->uuid == obj.uuid; }
  );
  objectsMap.erase(obj.uuid);
}

void Project::Scene::removeAllObjects() {
  objectsMap.clear();
  root.children.clear();
}

bool Project::Scene::moveObject(uint32_t uuidObject, uint32_t uuidTarget, bool asChild)
{
  if(uuidObject == uuidTarget) {
    return true;
  }

  auto objIt = objectsMap.find(uuidObject);
  auto targetIt = objectsMap.find(uuidTarget);
  if (objIt == objectsMap.end() || targetIt == objectsMap.end()) {
    return false;
  }

  auto obj = objIt->second;
  auto target = targetIt->second;

  // Remove from current parent
  if (obj->parent) {
    std::erase_if(
      obj->parent->children,
      [&obj](const std::shared_ptr<Object> &ref) { return ref->uuid == obj->uuid; }
    );
  }

  if (asChild) {
    // Add as child to target
    target->children.push_back(obj);
    obj->parent = target.get();
  } else {
    // Add as sibling to target
    auto parent = target->parent;
    if (parent) {
      // insert after target
      auto &siblings = parent->children;
      auto it = std::find_if(
        siblings.begin(), siblings.end(),
        [&target](const std::shared_ptr<Object> &ref) { return ref->uuid == target->uuid; }
      );
      if (it != siblings.end())
      {
        siblings.insert(it + 1, obj);
        obj->parent = parent;
      }
    }
  }

  return true;
}

void Project::Scene::save()
{
  Utils::FS::saveTextFile(scenePath + "/scene.json", serialize());
}

uint32_t Project::Scene::createPrefabFromObject(uint32_t uuid)
{
  auto obj = getObjectByUUID(uuid);
  if(!obj)return 0;

  Prefab prefab{};
  prefab.uuid.value = Utils::Hash::randomU64();
  auto prefabJson = prefab.serialize(*obj);

  std::string name = obj->name;

  name.erase(std::remove_if(name.begin(), name.end(),
    [](char c) { return !std::isalnum(c) && c != '_'; }
  ), name.end());
  if(name.empty())name = "prefab " + std::to_string(prefab.uuid.value);

  Utils::FS::saveTextFile(
    ctx.project->getPath() + "/assets/" + name + ".prefab",
    prefabJson
  );

  ctx.project->getAssets().reload();
  return 0;
}

std::string Project::Scene::serialize() {
  auto json = conf.serialize();
  auto graph = root.serialize();

  return std::string{"{\n"}
    + "\"conf\": " + json + ",\n"
    + "\"graph\": " + graph + "\n"
    + "}\n";
}

void Project::Scene::resetLayers()
{
  conf.layers3D.clear();
  conf.layersPtx.clear();
  conf.layers2D.clear();

  LayerConf layer{};
  layer.name.value = "3D Opaque";
  layer.depthCompare.value = true;
  layer.depthWrite.value = true;
  layer.blender.value = 0;
  conf.layers3D.push_back(layer);

  layer.name.value = "3D Transp.";
  layer.depthCompare.value = true;
  layer.depthWrite.value = false;
  layer.blender.value = RDPQ_BLENDER_MULTIPLY;
  conf.layers3D.push_back(layer);

  layer.name.value = "PTX Opaque";
  layer.depthCompare.value = true;
  layer.depthWrite.value = true;
  layer.blender.value = 0;
  conf.layersPtx.push_back(layer);

  layer.name.value = "2D";
  layer.depthCompare.value = false;
  layer.depthWrite.value = false;
  layer.blender.value = 0;
  conf.layers2D.push_back(layer);
}

void Project::Scene::deserialize(const std::string &data)
{
  if(data.empty())return;

  auto doc = Utils::JSON::load(data);
  if (!doc.is_object())return;

  auto docConf = doc["conf"];
  if (docConf.is_object()) {
    Utils::JSON::readProp(docConf, conf.name);
    conf.fbWidth = Utils::JSON::readInt(docConf, "fbWidth");
    conf.fbHeight = Utils::JSON::readInt(docConf, "fbHeight");
    conf.fbFormat = Utils::JSON::readInt(docConf, "fbFormat");
    conf.clearColor = Utils::JSON::readColor(docConf, "clearColor");
    Utils::JSON::readProp(docConf, conf.doClearColor);
    Utils::JSON::readProp(docConf, conf.doClearDepth);
    Utils::JSON::readProp(docConf, conf.renderPipeline);

    auto readLayer = [](const simdjson::simdjson_result<simdjson::dom::object>& dom) {
      LayerConf layer{};
      Utils::JSON::readProp(dom, layer.name);
      Utils::JSON::readProp(dom, layer.depthCompare, true);
      Utils::JSON::readProp(dom, layer.depthWrite, true);
      Utils::JSON::readProp(dom, layer.blender);
      return layer;
    };

    conf.layers3D  = Utils::JSON::readArray<LayerConf>(docConf, "layers3D", readLayer);
    conf.layersPtx = Utils::JSON::readArray<LayerConf>(docConf, "layersPtx", readLayer);
    conf.layers2D  = Utils::JSON::readArray<LayerConf>(docConf, "layers2D", readLayer);

    if(conf.layers3D.empty()) {
      resetLayers();
    }
  }

  removeAllObjects();
  auto docGraph = doc["graph"];
  root.deserialize(this, docGraph);
}
