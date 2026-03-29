/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Utils::JSON
{
  class Builder
  {
    public:
      nlohmann::json doc{};

      Builder() {

      }

      template<typename T>
      Builder& set(const std::string &key, T value) {
        doc[key] = value;
        return *this;
      }

      template<typename PROP>
      Builder& set(const Property<PROP> &prop) {
        set(prop.name, prop.value);
        return *this;
      }

      Builder& set(const std::string &key, const nlohmann::json &json) {
        doc[key] = json;
        return *this;
      }

      void set(const std::string &key, const glm::ivec2 &vec) {
        doc[key] = { vec.x, vec.y };
      }

      void set(const std::string &key, const glm::vec2 &vec) {
        doc[key] = { vec.x, vec.y };
      }

      void set(const std::string &key, const glm::vec3 &vec) {
        doc[key] = { vec.x, vec.y, vec.z };
      }


      Builder& set(const std::string &key, const glm::vec4 &vec) {
        doc[key] = { vec.x, vec.y, vec.z, vec.w };
        return *this;
      }

      void set(const std::string &key, const glm::quat &vec) {
        doc[key] = { vec.x, vec.y, vec.z, vec.w };
      }

      template<typename T>
      Builder& setArray(const std::string &key, const std::vector<T> &parts, std::function<void(Builder&, const T&)> cb) {
        auto arr = nlohmann::json::array();
        for (auto &part : parts) {
          Builder childBuilder{};
          cb(childBuilder, part);
          arr.push_back(childBuilder.doc);
        }
        doc[key] = arr;
        return *this;
      }

      std::string toString() const {
        return doc.dump(2);
      }
  };
}
