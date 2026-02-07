/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "fs.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Utils::JSON
{
  inline nlohmann::json loadFile(const std::string &path) {
    auto jsonData = FS::loadTextFile(path);
    if (jsonData.empty()) {
      return {};
    }
    return nlohmann::json::parse(jsonData);
  }

  inline nlohmann::json loadFile(const std::filesystem::path &path) {
    return  loadFile(path.string());
  }
  /*
  template<typename RES, typename T>
  inline std::vector<RES> readArray(
    const simdjson::simdjson_result<T> &el,
    const std::string &key,
    std::function<RES(const simdjson::simdjson_result<simdjson::dom::object>&)> cb
  ) {
    std::vector<RES> result{};
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return result;
    }
    auto arr = val.get_array();
    if (arr.error() != simdjson::SUCCESS) {
      return result;
    }
    for (auto item : *arr) {
      result.push_back(cb(item.get_object()));
    }
    return result;
  }
*/
  template<typename PROP>
  inline void readProp(const nlohmann::json &el, Property<PROP> &prop, const PROP& defValue = PROP{}) {
    PROP val{};
    if constexpr (std::is_same_v<PROP, bool>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, uint32_t>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, uint64_t>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, int32_t>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, int64_t>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, float>) {
      val = el.value(prop.name, defValue);
    } else if constexpr (std::is_same_v<PROP, glm::ivec2>) {
      val = !el.contains(prop.name) ? defValue
        : glm::ivec2{el[prop.name][0], el[prop.name][1]};
    } else if constexpr (std::is_same_v<PROP, glm::vec3>) {
      val = !el.contains(prop.name) ? defValue
        : glm::vec3{el[prop.name][0], el[prop.name][1], el[prop.name][2]};
    } else if constexpr (std::is_same_v<PROP, glm::vec4>) {
      val = !el.contains(prop.name) ? defValue
        : glm::vec4{el[prop.name][0], el[prop.name][1], el[prop.name][2], el[prop.name][3]};
    } else if constexpr (std::is_same_v<PROP, glm::quat>) {
      val = !el.contains(prop.name) ? glm::quat{0,0,0,1}
        : glm::quat{el[prop.name][0], el[prop.name][1], el[prop.name][2], el[prop.name][3]};
    } else if constexpr (std::is_same_v<PROP, std::string>) {
      val = el.value(prop.name, defValue);
    } else
    {
      static_assert(false, "Unsupported type in Utils::JSON::readProp");
    }

    prop.value = val;
  }
}
