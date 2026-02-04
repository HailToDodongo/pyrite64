/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "lang.h"

#include <stdexcept>
#include <string>

#include "json.hpp"
#include "../../utils/prop.h"
#include "../../utils/json.h"

namespace {
  static std::unordered_map<std::string, std::string> gMessages;
}

static void loadMessages(nlohmann::json &json, const std::string &prefix) {
  for (auto& [key, val] : json.items()) {
    if (val.type() == nlohmann::json::value_t::object) {
      loadMessages(val, prefix + key + ".");
    } else if (val.type() == nlohmann::json::value_t::string) {
      gMessages[prefix + key] = val;
    } else {
      throw std::runtime_error("Unsupported node type in language file!");
    }
  }
}

void Editor::applyLang(const std::string &lang)
{
  // Read translation file, fallback to english
  auto gFallbackJSON = Utils::JSON::loadFile("data/lang/messages_en.json");
  if (gFallbackJSON.empty()) {
    throw std::runtime_error("Not a valid fallback language file!");
  }
  loadMessages(gFallbackJSON, "");
  if (lang != "en") {
    std::string message_file = std::string("data/lang/messages_") + lang + ".json";
    printf("Opening message file: %s\n", message_file.c_str());
    auto gMessagesJSON = Utils::JSON::loadFile(message_file);
    if (gMessagesJSON.empty()) {
      throw std::runtime_error("Not a valid language file!");
    }
    loadMessages(gMessagesJSON, "");
  }
}

const char* Editor::message(const std::string &id) {
  if (gMessages.contains(id)) {
    return gMessages[id].c_str();
  }
  return id.c_str();
}

const std::string Editor::Message::ASSET_NONE_SELECTED = "asset.none_selected";
const std::string Editor::Message::ASSET_FILE = "asset.file";
const std::string Editor::Message::ASSET_SETTINGS = "asset.settings";
const std::string Editor::Message::IMAGE_COMBOBOX_FORMAT = "asset.image_format";
const std::string Editor::Message::MODEL_BASE_SCALE = "model.base_scale";
const std::string Editor::Message::MODEL_CREATE_BVH = "model.create_bvh";
const std::string Editor::Message::MODEL_COLLISION = "model.collision";
const std::string Editor::Message::SCENE_ON_BOOT = "scene.on_boot";
const std::string Editor::Message::SCENE_ON_RESET = "scene.on_reset";