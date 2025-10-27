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

namespace
{
  constinit uint64_t nextUUID{1};
}

std::string Project::SceneConf::serialize() const {

  Utils::JSON::Builder builder{};
  builder.set("name", name);
  builder.set("fbWidth", fbWidth);
  builder.set("fbHeight", fbHeight);
  builder.set("fbFormat", fbFormat);
  builder.set("clearColor", clearColor);
  builder.set("doClearColor", doClearColor);
  builder.set("doClearDepth", doClearDepth);
  return builder.toString();
}

Project::Scene::Scene(int id_, const std::string &projectPath)
  : id{id_}
{
  Utils::Logger::log("Loading scene: " + std::to_string(id));
  scenePath = projectPath + "/data/scenes/" + std::to_string(id);

  root.id = 0;
  root.name = "Scene";
  root.uuid = Utils::Hash::sha256_64bit(root.name);
  deserialize(Utils::FS::loadTextFile(scenePath + "/scene.json"));
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object &parent) {
  auto child = std::make_shared<Object>(parent);
  child->id = nextUUID++;
  child->name = "New Object ("+std::to_string(child->id)+")";
  child->uuid = Utils::Hash::sha256_64bit(child->name + std::to_string(rand()));
  child->scale = {1,1,1};
  child->rot = glm::identity<glm::quat>();
  return addObject(parent, child);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object&parent, std::shared_ptr<Object> obj) {
  parent.children.push_back(obj);
  objectsMap[obj->uuid] = obj;
  return obj;
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

void Project::Scene::save()
{
  Utils::FS::saveTextFile(scenePath + "/scene.json", serialize());
}

std::string Project::Scene::serialize() {
  auto json = conf.serialize();
  auto graph = root.serialize();

  return std::string{"{\n"}
    + "\"conf\": " + json + ",\n"
    + "\"graph\": " + graph + "\n"
    + "}\n";
}

void Project::Scene::deserialize(const std::string &data)
{
  if(data.empty())return;

  auto doc = Utils::JSON::load(data);
  if (!doc.is_object())return;

  auto docConf = doc["conf"];
  if (docConf.is_object()) {
    conf.name = Utils::JSON::readString(docConf, "name");
    conf.fbWidth = Utils::JSON::readInt(docConf, "fbWidth");
    conf.fbHeight = Utils::JSON::readInt(docConf, "fbHeight");
    conf.fbFormat = Utils::JSON::readInt(docConf, "fbFormat");
    conf.clearColor = Utils::JSON::readColor(docConf, "clearColor");
    conf.doClearColor = Utils::JSON::readBool(docConf, "doClearColor");
    conf.doClearDepth = Utils::JSON::readBool(docConf, "doClearDepth");
  }

  removeAllObjects();
  auto docGraph = doc["graph"];
  root.deserialize(*this, docGraph);
}
