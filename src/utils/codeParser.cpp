/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "codeParser.h"

#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>

#include "fs.h"
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
    if (str == "bool") return Utils::DataType::BOOL;
    if (str == "color_t") return Utils::DataType::COLOR;
    if (str == "char") return Utils::DataType::string;
    if (str == "fm_vec3_t") return Utils::DataType::VEC3;
    if (str == "fm_quat_t") return Utils::DataType::QUAT;
    if (str == "AssetRef<sprite_t>") return Utils::DataType::ASSET_SPRITE;
    if (str == "PrefabRef") return Utils::DataType::PREFAB;
    if (str == "ObjectRef" || str == "P64::ObjectRef" || str.rfind("ObjectRef<", 0) == 0 || str.find("::ObjectRef<") != std::string::npos) return Utils::DataType::OBJECT_REF;
    return Utils::DataType::s32;
  }

  constexpr uint32_t getTypeSize(Utils::DataType type) {
    switch(type) {
      case Utils::DataType::u8:
      case Utils::DataType::s8:
      case Utils::DataType::BOOL:
        return 1;
      case Utils::DataType::u16:
      case Utils::DataType::s16:
        return 2;
      case Utils::DataType::VEC3:
        return 12;
      case Utils::DataType::QUAT:
        return 16;
      case Utils::DataType::u32:
      case Utils::DataType::s32:
      case Utils::DataType::f32:
      case Utils::DataType::COLOR:
      case Utils::DataType::ASSET_SPRITE:
      case Utils::DataType::OBJECT_REF:
      case Utils::DataType::PREFAB:
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

  // Parses a "bit=name" comma separated list (e.g. "0=Fire, 1=Water") into (bit, name) pairs.
  std::vector<std::pair<int, std::string>> parseBitmask(const std::string& meta) {
    std::vector<std::pair<int, std::string>> result;
    std::stringstream ss(meta);
    std::string part;
    while (std::getline(ss, part, ',')) {
      auto eq = part.find('=');
      if (eq == std::string::npos) continue;
      try {
        result.push_back({std::stoi(trim(part.substr(0, eq))), trim(part.substr(eq + 1))});
      } catch (...) {}
    }
    return result;
  }

  // If 'i' sits on the opening quote of a string or char literal, advances it past the
  // closing quote (honoring backslash escapes) and returns true. Used by the parenthesis
  // scanners so brackets inside literals (e.g. P64::Name("foo(((")) don't affect nesting.
  bool skipLiteral(const std::string& text, size_t& i) {
    char quote = text[i];
    if (quote != '"' && quote != '\'') return false;
    ++i;
    while (i < text.size() && text[i] != quote) {
      if (text[i] == '\\') ++i;
      ++i;
    }
    if (i < text.size()) ++i; // closing quote
    return true;
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
          if (skipLiteral(text, i)) continue;
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

namespace fs = std::filesystem;

namespace
{
  std::string stripComments(const std::string &sourceCode)
  {
    auto code = std::regex_replace(sourceCode, std::regex(R"(//[^\n]*)"), "");
    return std::regex_replace(code, std::regex(R"(/\*[\s\S]*?\*/)"), "");
  }

  std::string trimSpace(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
  }

  // Returns the index after the bracket closing the one at 'start', or npos if unbalanced.
  size_t skipBlock(const std::string &text, size_t start, char open, char close)
  {
    size_t i = start + 1;
    int depth = 1;
    while (i < text.size()) {
      if (skipLiteral(text, i)) continue;
      if (text[i] == open) ++depth;
      else if (text[i] == close && --depth == 0) return i + 1;
      ++i;
    }
    return std::string::npos;
  }

  std::string removeAttributes(const std::string &text)
  {
    std::string res{};
    for (size_t i = 0; i < text.size();) {
      if (text.compare(i, 2, "[[") == 0) {
        auto end = text.find("]]", i);
        if (end == std::string::npos) break;
        i = end + 2;
        continue;
      }
      res += text[i++];
    }
    return res;
  }

  std::string firstWord(const std::string &text)
  {
    size_t end = 0;
    while (end < text.size() && (isalnum((unsigned char)text[end]) || text[end] == '_')) ++end;
    return text.substr(0, end);
  }

  bool isFunctionDecl(const std::string &stmt)
  {
    if (std::regex_search(stmt, std::regex(R"(\boperator\b)"))) return true;
    auto paren = stmt.find('(');
    if (paren == std::string::npos) return false;
    auto assign = stmt.find('=');
    return assign == std::string::npos || paren < assign;
  }

  /**
   * Reduces a struct/class body to its non-static data members, one per line.
   * Methods, nested types, access specifiers and static members are removed.
   */
  std::string extractDataMembers(const std::string &body)
  {
    static const std::unordered_set<std::string> SKIP_WORDS{
      "static", "using", "typedef", "friend", "template", "enum",
      "struct", "class", "union", "constexpr", "inline", "static_assert"
    };

    std::string res{}, stmt{};
    bool skipStmt = false;

    auto finishStmt = [&]() {
      auto plain = trimSpace(removeAttributes(stmt));
      bool isBitField = std::regex_search(plain, std::regex(R"((^|[^:]):([^:]|$))"));
      if (isBitField && !skipStmt) {
        Utils::Logger::log("Bit-fields in script data are not supported: '" + plain + "'", Utils::Logger::LEVEL_WARN);
      }
      if (!skipStmt && !isBitField && !plain.empty() && !SKIP_WORDS.contains(firstWord(plain)) && !isFunctionDecl(plain)) {
        res += trimSpace(stmt) + ";\n";
      }
      stmt.clear();
      skipStmt = false;
    };

    for (size_t i = 0; i < body.size();)
    {
      size_t litStart = i;
      if (skipLiteral(body, i)) {
        stmt += body.substr(litStart, i - litStart);
        continue;
      }

      char c = body[i];
      if (c == '{') {
        auto end = skipBlock(body, i, '{', '}');
        if (end == std::string::npos) break;
        auto plain = trimSpace(removeAttributes(stmt));
        auto word = firstWord(plain);

        if (word == "struct" || word == "class" || word == "union" || word == "enum") {
          skipStmt = true; // nested type, ignore up to the next ';'
        } else if (isFunctionDecl(plain)) {
          stmt.clear(); // method body, no ';' follows
        } else {
          stmt += body.substr(i, end - i); // brace initializer
        }
        i = end;
        continue;
      }

      if (c == ';') {
        finishStmt();
        ++i;
        continue;
      }

      bool isScope = (i + 1 < body.size() && body[i + 1] == ':') || (i > 0 && body[i - 1] == ':');
      if (c == ':' && !isScope) {
        auto plain = trimSpace(stmt);
        if (plain == "public" || plain == "private" || plain == "protected") {
          stmt.clear();
          ++i;
          continue;
        }
      }

      stmt += c;
      ++i;
    }
    return res;
  }

  std::optional<std::string> findStructBody(const std::string &code, const std::string &name)
  {
    std::regex declRegex("\\b(?:struct|class)\\s+" + name + "\\b");
    for (auto it = std::sregex_iterator(code.begin(), code.end(), declRegex); it != std::sregex_iterator(); ++it)
    {
      size_t pos = it->position() + it->length();
      while (pos < code.size() && isspace((unsigned char)code[pos])) ++pos;
      if (code.compare(pos, 5, "final") == 0) {
        pos += 5;
        while (pos < code.size() && isspace((unsigned char)code[pos])) ++pos;
      }

      if (pos < code.size() && code[pos] == ':') {
        Utils::Logger::log("Script data type '" + name + "' has base classes, which are not supported", Utils::Logger::LEVEL_WARN);
        return std::nullopt;
      }
      if (pos >= code.size() || code[pos] != '{') continue; // forward declaration or usage

      auto end = skipBlock(code, pos, '{', '}');
      if (end == std::string::npos) return std::nullopt;
      return code.substr(pos + 1, end - pos - 2);
    }
    return std::nullopt;
  }

  // Searches the struct in the code and then recursively in its quoted includes.
  std::optional<std::string> findStructInIncludes(
    const std::string &code, const std::string &name, const fs::path &dir,
    const std::vector<fs::path> &includeDirs, std::unordered_set<std::string> &visited, int depth
  ) {
    if (auto body = findStructBody(code, name)) return body;
    if (depth > 8) return std::nullopt;

    std::regex includeRegex(R"(#\s*include\s*"([^"]+)\")");
    for (auto it = std::sregex_iterator(code.begin(), code.end(), includeRegex); it != std::sregex_iterator(); ++it)
    {
      std::string inc = (*it)[1];
      std::vector<fs::path> candidates{dir / inc};
      for (auto &incDir : includeDirs) candidates.push_back(incDir / inc);

      for (auto &candidate : candidates)
      {
        std::error_code ec;
        if (!fs::is_regular_file(candidate, ec)) continue;
        auto key = fs::weakly_canonical(candidate, ec).string();
        if (!visited.insert(key).second) break;

        auto incCode = stripComments(Utils::FS::loadTextFile(candidate));
        if (auto body = findStructInIncludes(incCode, name, candidate.parent_path(), includeDirs, visited, depth + 1)) {
          return body;
        }
        break;
      }
    }
    return std::nullopt;
  }

  std::vector<Utils::CPP::Field> parseFields(const std::string &body)
  {
    using namespace Utils;
    using namespace Utils::CPP;
    std::vector<Field> fields{};
    std::string currentGroup{};
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

      // Pre-parse the bitmask attribute for unsigned int fields, so the editor doesn't re-parse each frame.
      if (field.type == DataType::u8 || field.type == DataType::u16 || field.type == DataType::u32) {
        auto bitmaskAttr = field.attr.find("P64::Bitmask");
        if (bitmaskAttr != field.attr.end()) {
          field.bitmask = parseBitmask(bitmaskAttr->second);
        }
      }

      // Pre-parse numeric bounds: Range(min, max) gives a slider, Min(x)/Max(x) only clamp.
      if (field.isNumeric()) {
        auto rangeAttr = field.attr.find("P64::Range");
        if (rangeAttr != field.attr.end()) {
          auto values = Utils::parseFloatList(rangeAttr->second);
          if (values.size() >= 2 && values[0] <= values[1]) {
            field.min = values[0];
            field.max = values[1];
            field.slider = true;
          } else {
            Logger::log("Invalid P64::Range on field '" + field.name + "', expected (min, max)", Logger::LEVEL_WARN);
          }
        }
        auto minAttr = field.attr.find("P64::Min");
        if (minAttr != field.attr.end()) {
          auto values = Utils::parseFloatList(minAttr->second);
          if (!values.empty()) field.min = values[0];
        }
        auto maxAttr = field.attr.find("P64::Max");
        if (maxAttr != field.attr.end()) {
          auto values = Utils::parseFloatList(maxAttr->second);
          if (!values.empty()) field.max = values[0];
        }
        if (field.min && field.max && *field.min > *field.max) {
          Logger::log("Field '" + field.name + "' has min > max, ignoring bounds", Logger::LEVEL_WARN);
          field.min.reset();
          field.max.reset();
          field.slider = false;
        }
      }

      // Normalize bool defaults ("true"/"1"/empty) into "0"/"1".
      if (field.type == DataType::BOOL) {
        auto def = trim(field.defaultValue);
        field.defaultValue = (def == "true" || def == "1") ? "1" : "0";
      }

      // Normalize color defaults (e.g. "{255, 0, 0, 255}" or empty) into "r,g,b,a".
      // Macros like RGBA32(...) can't be evaluated here and fall back to opaque white.
      if (field.type == DataType::COLOR) {
        auto def = trim(field.defaultValue);
        auto values = (def.empty() || def[0] != '{') ? std::vector<float>{} : Utils::parseFloatList(def);
        values.resize(4, 255.0f);
        for (auto &v : values) v = std::clamp(v, 0.0f, 255.0f);
        field.defaultValue = Utils::floatListToString(values.data(), values.size());
      }

      // Normalize vector defaults (e.g. "{{1, 2, 3}}" or empty) into the "x,y,z" form stored by the editor.
      if (field.type == DataType::VEC3 || field.type == DataType::QUAT) {
        auto values = Utils::parseFloatList(field.defaultValue);
        if (field.type == DataType::QUAT && values.empty()) values = {0,0,0,1};
        values.resize(field.type == DataType::VEC3 ? 3 : 4, 0.0f);
        field.defaultValue = Utils::floatListToString(values.data(), values.size());
      }

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

      auto groupAttr = field.attr.find("P64::Group");
      if (groupAttr != field.attr.end()) {
        currentGroup = groupAttr->second;
      }
      field.group = currentGroup;

      fields.push_back(field);
      fieldBegin = fieldMatch.suffix().first;
    }

    return fields;
  }
}

Utils::CPP::Struct Utils::CPP::parseDataStruct(const std::string &sourceCode, const std::string &structName)
{
  auto code = stripComments(sourceCode);

  // Locate the P64_DATA(...) body by matching parentheses, so defaults that
  // contain calls or macros (e.g. "color_t c = RGBA32(...)") don't end it early.
  auto bodyStart = code.find("P64_DATA(");
  if (bodyStart == std::string::npos)return {};

  bodyStart += std::string_view{"P64_DATA("}.size();
  size_t bodyEnd = bodyStart;
  int depth = 1;
  while (bodyEnd < code.size() && depth > 0) {
    if (skipLiteral(code, bodyEnd)) continue;
    if (code[bodyEnd] == '(') ++depth;
    else if (code[bodyEnd] == ')') --depth;
    ++bodyEnd;
  }
  if (depth != 0) return {};

  Struct s{.name = "Data"};
  if (s.name != structName) return {};

  s.fields = parseFields(code.substr(bodyStart, bodyEnd - 1 - bodyStart));
  return s;
}

Utils::CPP::Struct Utils::CPP::parseScriptData(
  const std::string &sourceCode, const fs::path &sourcePath, const std::vector<fs::path> &includeDirs
) {
  auto code = stripComments(sourceCode);

  constexpr std::string_view MACRO_TYPE{"P64_DATA_TYPE("};
  auto macroPos = code.find(MACRO_TYPE);
  if (macroPos == std::string::npos) {
    return parseDataStruct(sourceCode, "Data");
  }

  auto argStart = macroPos + MACRO_TYPE.size();
  auto argEnd = code.find(')', argStart);
  if (argEnd == std::string::npos) return {};

  auto typeName = trimSpace(code.substr(argStart, argEnd - argStart));
  auto nsPos = typeName.rfind("::");
  auto name = nsPos == std::string::npos ? typeName : typeName.substr(nsPos + 2);

  Struct s{.name = "Data"};
  if (!std::regex_match(name, std::regex(R"([A-Za-z_]\w*)"))) {
    Logger::log("Invalid script data type '" + typeName + "' in " + sourcePath.filename().string(), Logger::LEVEL_WARN);
    return s;
  }

  std::unordered_set<std::string> visited{};
  auto body = findStructInIncludes(code, name, sourcePath.parent_path(), includeDirs, visited, 0);
  if (!body) {
    Logger::log("Script data type '" + typeName + "' not found in " + sourcePath.filename().string() + " or its includes", Logger::LEVEL_WARN);
    return s;
  }

  s.fields = parseFields(extractDataMembers(*body));
  return s;
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

