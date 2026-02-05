/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "object.h"

#include "scene.h"
#include <algorithm>

#include "../../editor/imgui/lang.h"
#include "../../utils/hash.h"
#include "../../utils/jsonBuilder.h"
#include "../../utils/json.h"

using Builder = Utils::JSON::Builder;

namespace
{
  nlohmann::json serializeObj(const Project::Object &obj)
  {
    Builder builder{};
    builder.set("id", obj.id);
    builder.set("name", obj.name);
    builder.set("uuid", obj.uuid);

    builder.set("selectable", obj.selectable);
    builder.set("enabled", obj.enabled);

    builder
      .set(obj.uuidPrefab)
      .set(obj.pos)
      .set(obj.rot)
      .set(obj.scale);

    auto ovr = nlohmann::json::object();
    for(auto &[key, val] : obj.propOverrides) {
      ovr[std::to_string(key)] = val.serialize();
    }
    builder.doc["propOverrides"] = ovr;

    nlohmann::json comps = nlohmann::json::array();
    for (auto &comp : obj.components) {
      auto &def = Project::Component::TABLE[comp.id];
      nlohmann::json c{};
      c["id"] = comp.id;
      c["uuid"] = comp.uuid;
      c["name"] = comp.name;
      c["data"] = def.funcSerialize(comp);
      comps.push_back(c);
    }
    builder.doc["components"] = comps;

    nlohmann::json children = nlohmann::json::array();
    for (const auto &child : obj.children) {
      children.push_back(serializeObj(*child));
    }
    builder.set("children", children);
    return builder.doc;
  }
}

void Project::Object::addComponent(int compID) {
  if (compID < 0 || compID >= static_cast<int>(Component::TABLE.size()))return;
  auto &def = Component::TABLE[compID];

  components.push_back({
    .id = compID,
    .uuid = Utils::Hash::sha256_64bit(
      std::to_string(rand()) + std::to_string(compID)
    ),
    .name = std::string{Editor::message(def.name)},
    .data = def.funcInit(*this)
  });
}

void Project::Object::removeComponent(uint64_t uuid) {
  std::erase_if(
    components,
    [uuid](const Component::Entry &entry) {
      return entry.uuid == uuid;
    }
  );
}

nlohmann::json Project::Object::serialize() const {
  return serializeObj(*this);
}

void Project::Object::deserialize(Scene *scene, nlohmann::json &doc)
{
  if(!doc.is_object())return;

  id   = doc["id"];
  name = doc["name"];
  uuid = doc["uuid"];

  selectable = doc.value("selectable", true);
  enabled = doc.value("enabled", true);

  Utils::JSON::readProp(doc, uuidPrefab);
  Utils::JSON::readProp(doc, pos);
  Utils::JSON::readProp(doc, rot);
  Utils::JSON::readProp(doc, scale, {1,1,1});

  propOverrides.clear();
  if(doc.contains("propOverrides"))
  {
    auto &overrides = doc["propOverrides"];
    for (auto& [key, val] : overrides.items())
    {
      uint64_t keyInt = std::stoull(std::string(key));
      GenericValue v{};
      v.deserialize(val);
      propOverrides[keyInt] = v;
    }
  }

  if(doc.contains("components")) {
    auto &cmArray = doc["components"];
    int count = cmArray.size();
    for (int i=0; i<count; ++i) {
      auto compObj = cmArray.at(i);

      auto id = compObj["id"];
      if (id < 0 || id >= static_cast<int>(Component::TABLE.size()))continue;
      auto &def = Component::TABLE[id];

      components.push_back({
        .id = id,
        .uuid = compObj["uuid"],
        .name = compObj["name"],
        .data = def.funcDeserialize(compObj["data"])
      });

    }
  }

  if(!doc.contains("children"))return;

  auto &chArray = doc["children"];
  size_t childCount = chArray.size();

  assert(scene || childCount == 0);
  if(!scene)return;

  for (size_t i=0; i<childCount; ++i) {
    auto childObj = std::make_shared<Object>(*this);
    childObj->deserialize(scene, chArray[i]);
    scene->addObject(*this, childObj);
  }
}
