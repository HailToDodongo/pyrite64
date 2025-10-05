/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "simdjson.h"
#include "color.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Utils::JSON
{
  class Builder
  {
    private:
      simdjson::builder::string_builder builder{};
      bool hasData = false;

    public:
      Builder() {
        builder.start_object();
      }

      template<typename T>
      void set(const std::string &key, T value) {
        if (hasData) {
          builder.append_comma();
          builder.append_raw("\n");
        }
        builder.append_key_value(key, value);
        hasData = true;
      }

      void set(const std::string &key, const glm::vec3 &vec) {
        if (hasData)builder.append_comma();

        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        builder.start_array();
        builder.append(vec.x); builder.append_comma();
        builder.append(vec.y); builder.append_comma();
        builder.append(vec.z);
        builder.end_array();

        hasData = true;
      }

      void set(const std::string &key, const glm::quat &vec) {
        if (hasData)builder.append_comma();

        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        builder.start_array();
        builder.append(vec.x); builder.append_comma();
        builder.append(vec.y); builder.append_comma();
        builder.append(vec.z); builder.append_comma();
        builder.append(vec.w);
        builder.end_array();

        hasData = true;
      }

      void set(const std::string &key, const Color &col) {
        if (hasData)builder.append_comma();

        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        builder.start_array();
          builder.append(col.r); builder.append_comma();
          builder.append(col.g); builder.append_comma();
          builder.append(col.b); builder.append_comma();
          builder.append(col.a);
        builder.end_array();

        hasData = true;
      }

      void set(const std::string &key, Builder &build) {
        if (hasData)builder.append_comma();
        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        builder.append_raw(build.toString());
        hasData = true;
      }

      void set(const std::string &key, std::vector<Builder> &build) {
        if (hasData)builder.append_comma();
        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        builder.start_array();

        bool needsComma = false;
        for (auto &child : build) {
          if (needsComma) {
            builder.append_comma();
            builder.append_raw("\n");
          }
          builder.append_raw(child.toString());
          needsComma = true;
        }
        builder.end_array();

        hasData = true;
      }

      std::string toString() {
        return std::string{builder.c_str()} + "}";
      }
  };
}
