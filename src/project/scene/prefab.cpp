/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "prefab.h"

#include "../../utils/json.h"
#include "../../utils/jsonBuilder.h"
#include "../../context.h"

using Builder = Utils::JSON::Builder;

std::string Project::Prefab::serialize(const Object &obj) const
{
  Builder builder{};
  builder.set(uuid);
  builder.doc["obj"] = obj.serialize();
  return builder.toString();
}

void Project::Prefab::deserialize(const std::string &str)
{
  auto doc = nlohmann::json::parse(str, nullptr, false);
  if(!doc.is_object())return;
  Utils::JSON::readProp(doc, uuid);
  obj.deserialize(nullptr, doc["obj"]);
}

void Project::Prefab::save(const std::string &path)
{
  Utils::FS::saveTextFile(path, serialize());
}
