/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "binaryFile.h"

namespace Utils::CPP
{
  struct Field {
    DataType type;
    uint32_t dataSize;
    std::string name;
    std::unordered_map<std::string, std::string> attr;
    std::string defaultValue;
    // Parsed [[P64::Bitmask("0=Fire, 1=Water")]] entries as (bit-index, name) pairs.
    // Only populated for unsigned integer fields that carry the attribute.
    std::vector<std::pair<int, std::string>> bitmask;
    // Numeric bounds from [[P64::Range(min, max)]], [[P64::Min(x)]] or [[P64::Max(x)]].
    // Only populated for integer/float fields. 'slider' is set by Range, in which case
    // both bounds are present and the editor shows a slider instead of a clamped input.
    std::optional<float> min{};
    std::optional<float> max{};
    bool slider{false};
    std::string group{};

    [[nodiscard]] bool isNumeric() const {
      return type == DataType::u8 || type == DataType::s8 || type == DataType::u16 ||
             type == DataType::s16 || type == DataType::u32 || type == DataType::s32 ||
             type == DataType::f32;
    }
  };

  struct Struct {
    std::string name;
    std::vector<Field> fields;
  };

  Struct parseDataStruct(const std::string &sourceCode, const std::string &structName);

  /**
   * Parses the data of an object script, either inline ('P64_DATA') or a struct named by 'P64_DATA_TYPE'.
   * The struct is searched in the script itself and recursively in its quoted includes,
   * relative to the including file first and then to each of the include directories.
   */
  Struct parseScriptData(
    const std::string &sourceCode,
    const std::filesystem::path &sourcePath,
    const std::vector<std::filesystem::path> &includeDirs
  );

  bool hasFunction(const std::string &sourceCode, const std::string &retType, const std::string &name);

  uint32_t calcStructSize(const Struct &s);
}
