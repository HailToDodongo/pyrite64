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