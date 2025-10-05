/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "fs.h"
#include "simdjson.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Utils::JSON
{
  inline simdjson::dom::parser parser{};

  inline simdjson::simdjson_result<simdjson::dom::element> load(const std::string &json) {
    auto doc = parser.parse(std::string_view{json});
    return doc;
  }

  inline simdjson::simdjson_result<simdjson::dom::element> loadFile(const std::string &path) {
    auto jsonData = FS::loadTextFile(path);

    if (jsonData.empty()) {
      return {};
    }

    auto doc = parser.parse(std::string_view{jsonData});
    return doc;
  }

  inline std::string readString(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return "";
    }
    auto str = val.get_string();
    if (str.error() != simdjson::SUCCESS) {
      return "";
    }
    return std::string{str->data(), str->length()};
  }

  inline int readInt(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return 0;
    }
    auto i = val.get_int64();
    if (i.error() != simdjson::SUCCESS) {
      return 0;
    }
    return (int)(*i);
  }

  inline uint64_t readU64(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return 0;
    }
    auto i = val.get_uint64();
    if (i.error() != simdjson::SUCCESS) {
      return 0;
    }
    return *i;
  }

  inline float readFloat(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return 0.0f;
    }
    auto f = val.get_double();
    if (f.error() != simdjson::SUCCESS) {
      return 0.0f;
    }
    return (float)(*f);
  }

  inline bool readBool(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return false;
    }
    auto b = val.get_bool();
    if (b.error() != simdjson::SUCCESS) {
      return false;
    }
    return *b;
  }

  inline Color readColor(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    Color col{};
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS) {
      return col;
    }
    auto arr = val.get_array();
    if (arr.error() != simdjson::SUCCESS) {
      return col;
    }

    col.r = arr.at(0).get_double();
    col.g = arr.at(1).get_double();
    col.b = arr.at(2).get_double();
    col.a = arr.at(3).get_double();
    return col;
  }

  inline glm::vec3 readVec3(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key, const glm::vec3 &def = {}) {
    auto val = el[key];
    if (val.error() != simdjson::SUCCESS)return def;
    auto arr = val.get_array();
    if (arr.error() != simdjson::SUCCESS)return def;

    return {
      (float)arr.at(0).get_double(),
      (float)arr.at(1).get_double(),
      (float)arr.at(2).get_double()
    };
  }

  inline glm::quat readQuat(const simdjson::simdjson_result<simdjson::dom::element> &el, const std::string &key) {
    auto res = glm::identity<glm::quat>();

    auto val = el[key];
    if (val.error() != simdjson::SUCCESS)return res;
    auto arr = val.get_array();
    if (arr.error() != simdjson::SUCCESS)return res;

    res.x = arr.at(0).get_double();
    res.y = arr.at(1).get_double();
    res.z = arr.at(2).get_double();
    res.w = arr.at(3).get_double();
    return res;
  }
}
