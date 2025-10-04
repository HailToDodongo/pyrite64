/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include "scene.h"
#include <SDL3/SDL.h>
#include "simdjson.h"
#include "../../utils/json.h"
#include "../../context.h"

namespace
{
  std::string getConfPath(int id) {
    auto scenesPath = ctx.project->getPath() + "/data/scenes";
    return scenesPath + "/" + std::to_string(id) + "/conf.json";
  }
}

std::string Project::SceneConf::serialize() const {
  simdjson::builder::string_builder builder{};
  builder.start_object();
  builder.append_key_value<"name">(name);
  builder.append_comma();
  builder.append_key_value<"fbWidth">(fbWidth);
  builder.append_comma();
  builder.append_key_value<"fbHeight">(fbHeight);
  builder.append_comma();
  builder.append_key_value<"fbFormat">(fbFormat);
  builder.append_comma();

  builder.escape_and_append_with_quotes("clearColor");
  builder.append_colon();
  builder.start_array();
    builder.append(clearColor.r); builder.append_comma();
    builder.append(clearColor.g); builder.append_comma();
    builder.append(clearColor.b); builder.append_comma();
    builder.append(clearColor.a);
  builder.end_array();
  builder.append_comma();

  builder.append_key_value<"doClearColor">(doClearColor);
  builder.append_comma();
  builder.append_key_value<"doClearDepth">(doClearDepth);
  builder.end_object();
  return {builder.c_str()};
}

Project::Scene::Scene(int id_)
  : id{id_}
{
  printf("Load scene %d\n", id);

  auto doc = Utils::JSON::loadFile(getConfPath(id));
  if (doc.is_object()) {
    JSON_GET_STR(name);
    JSON_GET_INT(fbWidth);
    JSON_GET_INT(fbHeight);
    JSON_GET_INT(fbFormat);
    //JSON_GET_FLOAT(clearColor);

    auto clearColor = doc["clearColor"];
    if (!clearColor.error()) {
      auto col = clearColor.get_array();
      conf.clearColor.r = col.at(0).get_double();
      conf.clearColor.g = col.at(1).get_double();
      conf.clearColor.b = col.at(2).get_double();
      conf.clearColor.a = col.at(3).get_double();
    }
    JSON_GET_BOOL(doClearColor);
    JSON_GET_BOOL(doClearDepth);
  }
}

void Project::Scene::save()
{
  auto pathConfig = getConfPath(id);
  auto json = conf.serialize();
  SDL_SaveFile(pathConfig.c_str(), json.c_str(), json.size());
}
