/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "codeParser.h"

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

#include "logger.h"

namespace
{
  constexpr Utils::DataType fromString(const std::string &str)
  {
    if (str == "uint8_t") return Utils::DataType::u8;
    if (str == "int8_t") return Utils::DataType::s8;
    if (str == "uint16_t") return Utils::DataType::u16;
    if (str == "int16_t") return Utils::DataType::s16;
    if (str == "uint32_t") return Utils::DataType::u32;
    if (str == "int32_t") return Utils::DataType::s32;
    if (str == "float") return Utils::DataType::f32;
    if (str == "char") return Utils::DataType::string;
    if (str == "AssetRef<sprite_t>") return Utils::DataType::ASSET_SPRITE;
    if (str == "ObjectRef" || str == "P64::ObjectRef" || str.rfind("ObjectRef<", 0) == 0 || str.find("::ObjectRef<") != std::string::npos) return Utils::DataType::OBJECT_REF;
    return Utils::DataType::s32;
  }

  constexpr uint32_t getTypeSize(Utils::DataType type) {
    switch(type) {
      case Utils::DataType::u8:
      case Utils::DataType::s8:
        return 1;
      case Utils::DataType::u16:
      case Utils::DataType::s16:
        return 2;
      case Utils::DataType::u32:
      case Utils::DataType::s32:
      case Utils::DataType::f32:
      case Utils::DataType::ASSET_SPRITE:
      case Utils::DataType::OBJECT_REF:
      default:
        return 4;
    }
  }

  std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\"");
    size_t end = s.find_last_not_of(" \t\n\r\"");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
  }

  std::unordered_map<std::string, std::string> parseAttributes(const std::string& attrText) {
    std::unordered_map<std::string, std::string> result;
    std::string text = trim(attrText);
    if (text.empty()) return result;

    size_t i = 0;
    while (i < text.size()) {
      // Extract attribute name
      size_t nameStart = i;
      while (i < text.size() && text[i] != '(' && text[i] != ',' && text[i] != ']')
        ++i;
      std::string name = trim(text.substr(nameStart, i - nameStart));

      std::string value;
      if (i < text.size() && text[i] == '(') {
        ++i;
        int depth = 1;
        size_t valStart = i;
        while (i < text.size() && depth > 0) {
          if (text[i] == '(') depth++;
          else if (text[i] == ')') depth--;
          ++i;
        }
        value = trim(text.substr(valStart, i - valStart - 1));
      }

      result[name] = value;

      // Skip comma and whitespace
      while (i < text.size() && (text[i] == ',' || isspace((unsigned char)text[i])))
        ++i;
    }

    return result;
  }
}

Utils::CPP::Struct Utils::CPP::parseDataStruct(const std::string &sourceCode, const std::string &structName)
{
  // remove line and multi-line comments
  auto code = std::regex_replace(sourceCode, std::regex(R"(//[^\n]*)"), "");
  code = std::regex_replace(code, std::regex(R"(/\*[\s\S]*?\*/)"), "");

  std::vector<Struct> structs{};

  // match all structs to get the body of it
  std::regex structRegex(R"(P64_DATA\(([\s\S]*?)\);)");

  std::smatch structMatch;
  auto structBegin = code.cbegin();

  while (std::regex_search(structBegin, code.cend(), structMatch, structRegex))
  {
    Struct s{.name = "Data"};
    if (s.name != structName)continue;

    std::string body = structMatch[1];

    // Regex for attributes + field lines
    std::regex fieldRegex(
      R"((\[\[\s*([^\]]+)\s*\]\]\s*)?([\w:<>]+)\s+(\w+)(\[[0-9]+\])?(?:\s*\=(.*))?\s*;)"
    );

    std::smatch fieldMatch;
    auto fieldBegin = body.cbegin();

    while (std::regex_search(fieldBegin, body.cend(), fieldMatch, fieldRegex))
    {
      Field field{
        .type = fromString(fieldMatch[3]),
        .dataSize = getTypeSize(fromString(fieldMatch[3])),
        .name = fieldMatch[4],
        .attr = parseAttributes(fieldMatch[2]),
        .defaultValue = fieldMatch[6],
      };

      if(field.type == DataType::string) {
        try
        {
          auto strSize = fieldMatch[5].str(); // -> [42]
          field.dataSize = std::stoul(strSize.substr(1, strSize.size() - 2)); // parse without brackets
        } catch(...) {
          Logger::log(
            "Failed to parse size for string field: " + field.name + ", defaulting to 4 bytes.",
            Logger::LEVEL_ERROR
          );
          field.dataSize = 4;
        }

      }

      s.fields.push_back(field);
      fieldBegin = fieldMatch.suffix().first;
    }

    //structs.push_back(std::move(s));
    return s;
    // structBegin = structMatch.suffix().first;
  }

  return {};
}

bool Utils::CPP::hasFunction(const std::string&sourceCode, const std::string&retType, const std::string&name) {
  // remove line and multi-line comments
  auto code = std::regex_replace(sourceCode, std::regex(R"(//[^\n]*)"), "");
  code = std::regex_replace(code, std::regex(R"(/\*[\s\S]*?\*/)"), "");
  // remove all spaces and newlines
  code = std::regex_replace(code, std::regex(R"(\s+)"), "");

  auto expected = retType + name + "(";
  return code.find(expected) != std::string::npos;
}

uint32_t Utils::CPP::calcStructSize(const Struct&s) {
  uint32_t size = 0;
  for(const auto &field : s.fields) {
    size += field.dataSize;
  }
  return size;
}

