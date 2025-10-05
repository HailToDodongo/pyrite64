/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "object.h"

#include "scene.h"
#include "../../utils/jsonBuilder.h"
#include "../../utils/json.h"

using Builder = Utils::JSON::Builder;

namespace
{
  Builder serializeObj(Project::Object &obj)
  {
    Builder builder{};
    builder.set("id", obj.id);
    builder.set("name", obj.name);
    builder.set("uuid", obj.uuid);

    builder.set("pos", obj.pos);
    builder.set("rot", obj.rot);
    builder.set("scale", obj.scale);

    std::vector<Builder> children{};
    for (const auto &child : obj.children) {
      children.push_back(serializeObj(*child));
    }
    builder.set("children", children);
    return builder;
  }
}

std::string Project::Object::serialize() {
  return serializeObj(*this).toString();
}

void Project::Object::deserialize(Scene &scene, const simdjson::simdjson_result<simdjson::dom::element>&doc)
{
  if(!doc.is_object())return;

  id = Utils::JSON::readInt(doc, "id");
  name = Utils::JSON::readString(doc, "name");
  uuid = Utils::JSON::readU64(doc, "uuid");

  pos = Utils::JSON::readVec3(doc, "pos");
  rot = Utils::JSON::readQuat(doc, "rot");
  scale = Utils::JSON::readVec3(doc, "scale", {1,1,1});

  auto ch = doc["children"];
  if (ch.error() != simdjson::SUCCESS)return;
  auto chArray = ch.get_array();
  if (chArray.error() != simdjson::SUCCESS)return;

  size_t childCount = chArray.size();
  for (size_t i=0; i<childCount; ++i) {
    auto childObj = std::make_shared<Object>(*this);
    childObj->deserialize(scene, chArray.at(i));
    scene.addObject(*this, childObj);
  }
}
