/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene.h"
#include "object.h"
#include "../../utils/json.h"
#include "../../context.h"
#include "../../utils/hash.h"
#include "../../utils/jsonBuilder.h"
#include "../../utils/logger.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "../graph/nodes/nodeObjDel.h"
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
  constexpr float DEF_MODEL_SCALE = 1.0f;
}

nlohmann::json Project::SceneConf::serialize() const {

  auto writeLayer = [](Utils::JSON::Builder &b, const LayerConf &layer) {
    b.set(layer.name);
    b.set(layer.depthCompare);
    b.set(layer.depthWrite);
    b.set(layer.blender);
    b.set(layer.fog);
    b.set(layer.fogColorMode);
    b.set(layer.fogColor);
    b.set(layer.fogMin);
    b.set(layer.fogMax);
  };

  Utils::JSON::Builder builder{};
  builder.set(name)
    .set("fbWidth", fbWidth)
    .set("fbHeight", fbHeight)
    .set("fbFormat", fbFormat)
    .set(clearColor)
    .set(doClearColor)
    .set(doClearDepth)
    .set(renderPipeline)
    .set(frameLimit)
    .set(filter)
    .setArray<LayerConf>("layers3D", layers3D, writeLayer)
    .setArray<LayerConf>("layersPtx", layersPtx, writeLayer)
    .setArray<LayerConf>("layers2D", layers2D, writeLayer);

  return builder.doc;
}

Project::Scene::Scene(int id_, const std::string &projectPath)
  : id{id_}
{
  Utils::Logger::log("Loading scene: " + std::to_string(id));
  scenePath = projectPath + "/data/scenes/" + std::to_string(id);

  deserialize(Utils::FS::loadTextFile(scenePath + "/scene.json"));

  root.id = 0;
  root.name = "Scene";
  root.uuid = Utils::Hash::sha256_64bit(root.name);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(std::string &objJson, uint64_t parentUUID)
{
  auto p = getObjectByUUID(parentUUID);
  Object *parent = p ? p.get() : &root;

  auto json = nlohmann::json::parse(objJson, nullptr, false);
  auto obj = std::make_shared<Object>(*parent);
  obj->deserialize(this, json);
  return addObject(*parent, obj, true);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object &parent) {
  auto child = std::make_shared<Object>(parent);
  child->name = "New Object";
  child->scale.value = {DEF_MODEL_SCALE, DEF_MODEL_SCALE, DEF_MODEL_SCALE};
  child->rot.value = {0,0,0,1};
  return addObject(parent, child, true);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object&parent, std::shared_ptr<Object> obj, bool generateIDs) {
  parent.children.push_back(obj);

  auto setChildUUIDs = [this, generateIDs](const std::shared_ptr<Object> &objChild, auto& setChildUIDsRef) -> void
  {
    if(generateIDs)
    {
      objChild->id = getFreeObjectId();
      objChild->uuid = Utils::Hash::randomU64();
    }

    objectsMap[objChild->uuid] = objChild;
    for(const auto& child : objChild->children) {
      setChildUIDsRef(child, setChildUIDsRef);
    }
  };

  setChildUUIDs(obj, setChildUUIDs);
  return obj;
}

std::shared_ptr<Project::Object> Project::Scene::addPrefabInstance(uint64_t prefabUUID)
{
  auto prefab = ctx.project->getAssets().getPrefabByUUID(prefabUUID);
  if (!prefab)return nullptr;

  auto obj = std::make_shared<Object>(root);
  obj->id = getFreeObjectId();
  obj->name += prefab->obj.name + " ("+std::to_string(obj->id)+")";
  obj->uuid = Utils::Hash::randomU32();
  obj->pos = prefab->obj.pos;
  obj->rot = prefab->obj.rot;
  obj->scale = prefab->obj.scale;

  obj->uuidPrefab.value = prefab->uuid.value; // Link to prefab
  obj->addPropOverride(obj->pos); // by default allow transforming the instance
  obj->addPropOverride(obj->rot);
  obj->addPropOverride(obj->scale);

  return addObject(root, obj);
}

void Project::Scene::removeObject(Object &obj) {
  ctx.removeObjectSelection(obj.uuid);

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

std::string Project::Scene::serialize(bool minify) {
  nlohmann::json doc{};
  doc["conf"] = conf.serialize();
  doc["graph"] = root.serialize();
  return doc.dump(minify ? -1 : 2);
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

  auto doc = nlohmann::json::parse(data, nullptr, false);
  if (!doc.is_object())return;

  auto &docConf = doc["conf"];
  {
    Utils::JSON::readProp(docConf, conf.name);
    conf.fbWidth = docConf.value("fbWidth", 320);
    conf.fbHeight = docConf.value("fbHeight", 240);
    conf.fbFormat = docConf.value("fbFormat", 0);
    Utils::JSON::readProp(docConf, conf.clearColor);
    Utils::JSON::readProp(docConf, conf.doClearColor);
    Utils::JSON::readProp(docConf, conf.doClearDepth);
    Utils::JSON::readProp(docConf, conf.renderPipeline);
    Utils::JSON::readProp(docConf, conf.frameLimit, 0);
    Utils::JSON::readProp(docConf, conf.filter, 0);

    auto readLayer = [](const nlohmann::json &dom) {
      LayerConf layer{};
      Utils::JSON::readProp(dom, layer.name);
      Utils::JSON::readProp(dom, layer.depthCompare, true);
      Utils::JSON::readProp(dom, layer.depthWrite, true);
      Utils::JSON::readProp(dom, layer.blender);
      Utils::JSON::readProp(dom, layer.fog, false);
      Utils::JSON::readProp(dom, layer.fogColorMode, 0u);
      Utils::JSON::readProp(dom, layer.fogColor);
      Utils::JSON::readProp(dom, layer.fogMin, 0.0f);
      Utils::JSON::readProp(dom, layer.fogMax, 0.0f);


      return layer;
    };

    conf.layers3D.clear();
    conf.layersPtx.clear();
    conf.layers2D.clear();
    for(auto &item : docConf["layers3D"]) {
      conf.layers3D.push_back(readLayer(item));
    }
    for(auto &item : docConf["layersPtx"]) {
      conf.layersPtx.push_back(readLayer(item));
    }
    for(auto &item : docConf["layers2D"]) {
      conf.layers2D.push_back(readLayer(item));
    }
    if(conf.layers3D.empty()) {
      resetLayers();
    }
  }

  removeAllObjects();
  auto docGraph = doc["graph"];
  root.deserialize(this, docGraph);
}

uint16_t Project::Scene::getFreeObjectId()
{
  uint16_t objId = 1;

  for(int i=0; i<0xFFFF; ++i) {
    bool found = false;
    for (auto &[uuid, obj] : objectsMap) {
      if (obj->id == objId) {
        found = true;
        break;
      }
    }
    if (!found)break;
    ++objId;
  }
  return objId;
}
