/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "doctest/doctest.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "utils/codeParser.h"
#include "utils/logger.h"

namespace fs = std::filesystem;
using Utils::DataType;
using Utils::CPP::Field;
using Utils::CPP::Struct;

namespace
{
  // Creates a unique directory for files used by include tests, deleted again at the end.
  struct TempDir
  {
    fs::path path{};

    TempDir() {
      static std::atomic_int counter{0};
      static const auto runId = std::random_device{}();
      path = fs::temp_directory_path() / ("p64_test_" + std::to_string(runId) + "_" + std::to_string(counter++));
      fs::create_directories(path);
    }

    ~TempDir() {
      std::error_code ec;
      fs::remove_all(path, ec);
    }

    fs::path write(const std::string &relPath, const std::string &content) const {
      auto filePath = path / relPath;
      fs::create_directories(filePath.parent_path());
      std::ofstream{filePath} << content;
      return filePath;
    }
  };

  Struct parseData(const std::string &body) {
    return Utils::CPP::parseDataStruct("P64_DATA(\n" + body + "\n);", "Data");
  }

  Struct parseScript(const TempDir &dir, const std::string &code, const std::vector<fs::path> &includeDirs = {}) {
    auto path = dir.write("src/user/Script.cpp", code);
    return Utils::CPP::parseScriptData(code, path, includeDirs);
  }

  std::vector<std::string> fieldNames(const Struct &s) {
    std::vector<std::string> res{};
    for(auto &f : s.fields)res.push_back(f.name);
    return res;
  }

  std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t");
    auto end = s.find_last_not_of(" \t");
    return start == std::string::npos ? "" : s.substr(start, end - start + 1);
  }

  bool logContains(const std::string &text) {
    return Utils::Logger::getLog().find(text) != std::string::npos;
  }
}

TEST_SUITE("parseDataStruct")
{
  TEST_CASE("types and sizes")
  {
    auto s = parseData(R"(
      uint8_t a; int8_t b; uint16_t c; int16_t d; uint32_t e; int32_t f;
      float g; bool h; color_t i; fm_vec3_t j; fm_quat_t k;
      AssetRef<sprite_t> l; ObjectRef m; P64::ObjectRef n; ObjectRef<Foo> o; PrefabRef p;
      char str[12];
    )");

    struct Expected { const char* name; DataType type; uint32_t size; };
    std::vector<Expected> expected{
      {"a", DataType::u8, 1}, {"b", DataType::s8, 1}, {"c", DataType::u16, 2}, {"d", DataType::s16, 2},
      {"e", DataType::u32, 4}, {"f", DataType::s32, 4}, {"g", DataType::f32, 4}, {"h", DataType::BOOL, 1},
      {"i", DataType::COLOR, 4}, {"j", DataType::VEC3, 12}, {"k", DataType::QUAT, 16},
      {"l", DataType::ASSET_SPRITE, 4}, {"m", DataType::OBJECT_REF, 4}, {"n", DataType::OBJECT_REF, 4},
      {"o", DataType::OBJECT_REF, 4}, {"p", DataType::PREFAB, 4}, {"str", DataType::string, 12},
    };

    CHECK(s.name == "Data");
    REQUIRE(s.fields.size() == expected.size());
    for(size_t i=0; i<expected.size(); ++i) {
      CAPTURE(expected[i].name);
      CHECK(s.fields[i].name == expected[i].name);
      CHECK(s.fields[i].type == expected[i].type);
      CHECK(s.fields[i].dataSize == expected[i].size);
    }
    CHECK(Utils::CPP::calcStructSize(s) == 83);
  }

  TEST_CASE("unknown types fall back to 32-bit integers")
  {
    auto s = parseData("Coll::RaycastHit hit;");
    REQUIRE(s.fields.size() == 1);
    CHECK(s.fields[0].type == DataType::s32);
    CHECK(s.fields[0].dataSize == 4);
  }

  TEST_CASE("missing or mismatching data block")
  {
    CHECK(Utils::CPP::parseDataStruct("namespace X {}", "Data").fields.empty());
    CHECK(Utils::CPP::parseDataStruct("P64_DATA(int32_t a;);", "Other").name.empty());
    CHECK(Utils::CPP::parseDataStruct("P64_DATA(int32_t a;", "Data").fields.empty());
  }

  TEST_CASE("comments are ignored")
  {
    auto s = parseData(R"(
      int32_t a; // int32_t ghostA;
      /* int32_t ghostB;
         int32_t ghostC; */
      int32_t b;
    )");
    CHECK(fieldNames(s) == std::vector<std::string>{"a", "b"});
  }

  TEST_CASE("literals and nested parentheses don't end the block")
  {
    auto s = parseData(R"x(
      [[P64::Name("Odd (name; ))")]]
      color_t c = RGBA32(1, 2, 3, 4);
      int32_t after;
    )x");
    REQUIRE(fieldNames(s) == std::vector<std::string>{"c", "after"});
    CHECK(s.fields[0].attr.at("P64::Name") == "Odd (name; ))");
  }

  TEST_CASE("range, min and max attributes")
  {
    auto s = parseData(R"(
      [[P64::Range(0, 10)]] int32_t ranged;
      [[P64::Min(-2.5)]] float minOnly;
      [[P64::Max(7)]] uint8_t maxOnly;
      [[P64::Range(5, 1)]] int32_t invalid;
      [[P64::Range(0, 1)]] bool notNumeric;
    )");
    REQUIRE(s.fields.size() == 5);

    CHECK(s.fields[0].min == 0.0f);
    CHECK(s.fields[0].max == 10.0f);
    CHECK(s.fields[0].slider);

    CHECK(s.fields[1].min == -2.5f);
    CHECK_FALSE(s.fields[1].max.has_value());
    CHECK_FALSE(s.fields[1].slider);

    CHECK_FALSE(s.fields[2].min.has_value());
    CHECK(s.fields[2].max == 7.0f);

    CHECK_FALSE(s.fields[3].min.has_value());
    CHECK_FALSE(s.fields[3].slider);

    CHECK_FALSE(s.fields[4].min.has_value());
  }

  TEST_CASE("groups apply until the next group")
  {
    auto s = parseData(R"(
      int32_t before;
      [[P64::Group("Movement"), P64::Name("Speed")]] float speed;
      float jump;
      [[P64::Group("Debug")]] bool showPath;
      [[P64::Group("")]] int32_t after;
    )");
    REQUIRE(s.fields.size() == 5);
    CHECK(s.fields[0].group == "");
    CHECK(s.fields[1].group == "Movement");
    CHECK(s.fields[1].attr.at("P64::Name") == "Speed");
    CHECK(s.fields[2].group == "Movement");
    CHECK(s.fields[3].group == "Debug");
    CHECK(s.fields[4].group == "");
  }

  TEST_CASE("bitmask attribute")
  {
    auto s = parseData(R"(
      [[P64::Bitmask("0=Fire, 1=Water, 4=Earth")]] uint8_t flags;
      [[P64::Bitmask("0=Fire")]] int32_t signedFlags;
    )");
    REQUIRE(s.fields.size() == 2);

    std::vector<std::pair<int, std::string>> expected{{0, "Fire"}, {1, "Water"}, {4, "Earth"}};
    CHECK(s.fields[0].bitmask == expected);
    CHECK(s.fields[1].bitmask.empty());
  }

  TEST_CASE("default values")
  {
    auto s = parseData(R"(
      int32_t num = 42;
      bool boolTrue = true;
      bool boolOne = 1;
      bool boolNone;
      color_t col = {255, 0, 128, 64};
      color_t colNone;
      color_t colMacro = RGBA32(1, 2, 3, 4);
      fm_vec3_t vec = {{1, 2.5, 3}};
      fm_vec3_t vecNone;
      fm_quat_t quatNone;
    )");
    REQUIRE(s.fields.size() == 10);

    CHECK(trim(s.fields[0].defaultValue) == "42");
    CHECK(s.fields[1].defaultValue == "1");
    CHECK(s.fields[2].defaultValue == "1");
    CHECK(s.fields[3].defaultValue == "0");
    CHECK(s.fields[4].defaultValue == "255,0,128,64");
    CHECK(s.fields[5].defaultValue == "255,255,255,255");
    CHECK(s.fields[6].defaultValue == "255,255,255,255");
    CHECK(s.fields[7].defaultValue == "1,2.5,3");
    CHECK(s.fields[8].defaultValue == "0,0,0");
    CHECK(s.fields[9].defaultValue == "0,0,0,1");
  }
}

TEST_SUITE("parseScriptData")
{
  TEST_CASE("inline data block")
  {
    TempDir dir{};
    auto s = parseScript(dir, "namespace P64::Script::C1 { P64_DATA(int32_t a; float b;); }");
    CHECK(s.name == "Data");
    CHECK(fieldNames(s) == std::vector<std::string>{"a", "b"});
  }

  TEST_CASE("struct in the script itself")
  {
    TempDir dir{};
    auto s = parseScript(dir, R"(
      namespace User { struct PlayerData { int32_t health; float speed; }; }
      namespace P64::Script::C1 { P64_DATA_TYPE(User::PlayerData); }
    )");
    CHECK(s.name == "Data");
    CHECK(fieldNames(s) == std::vector<std::string>{"health", "speed"});
  }

  TEST_CASE("struct from includes")
  {
    TempDir dir{};
    dir.write("src/user/shared/PlayerData.h", R"(
      #pragma once
      #include "../Base.h"
      namespace User { struct PlayerData { [[P64::Name("Health")]] int32_t health = 100; }; }
    )");
    dir.write("src/user/Base.h", "#pragma once\nstruct Other { int32_t x; };\n");
    dir.write("src/common/Nested.h", "#pragma once\n#include \"Deep.h\"\n");
    dir.write("src/common/Deep.h", "#pragma once\nstruct DeepData { uint8_t a; uint8_t b; };\n");

    SUBCASE("relative to the script") {
      auto s = parseScript(dir, R"(
        #include "script/userScript.h"
        #include "shared/PlayerData.h"
        namespace P64::Script::C1 { P64_DATA_TYPE(User::PlayerData); }
      )");
      REQUIRE(fieldNames(s) == std::vector<std::string>{"health"});
      CHECK(s.fields[0].attr.at("P64::Name") == "Health");
      CHECK(trim(s.fields[0].defaultValue) == "100");
    }

    SUBCASE("found through an include of an include") {
      auto s = parseScript(dir, R"(
        #include "shared/PlayerData.h"
        namespace P64::Script::C1 { P64_DATA_TYPE(Other); }
      )");
      CHECK(fieldNames(s) == std::vector<std::string>{"x"});
    }

    SUBCASE("relative to an include directory, nested") {
      auto s = parseScript(dir, R"(
        #include "common/Nested.h"
        namespace P64::Script::C1 { P64_DATA_TYPE(DeepData); }
      )", {dir.path / "src"});
      CHECK(fieldNames(s) == std::vector<std::string>{"a", "b"});
    }

    SUBCASE("not found without the include directory") {
      Utils::Logger::clear();
      auto s = parseScript(dir, R"(
        #include "common/Nested.h"
        namespace P64::Script::C1 { P64_DATA_TYPE(DeepData); }
      )");
      CHECK(s.name == "Data");
      CHECK(s.fields.empty());
      CHECK(logContains("'DeepData' not found"));
    }
  }

  TEST_CASE("declarations other than the definition are skipped")
  {
    TempDir dir{};
    auto s = parseScript(dir, R"(
      struct PlayerData;
      void use(struct PlayerData *p);
      class PlayerData final { public: int32_t a; };
      namespace P64::Script::C1 { P64_DATA_TYPE(PlayerData); }
    )");
    CHECK(fieldNames(s) == std::vector<std::string>{"a"});
  }

  TEST_CASE("only data members are extracted")
  {
    TempDir dir{};
    auto s = parseScript(dir, R"(
      struct PlayerData
      {
        enum class State : uint8_t { IDLE, RUN };
        struct Inner { int32_t innerA; int32_t innerB; };
        union U { int32_t ua; float ub; };
        static constexpr int32_t MAX = 4;
        static int32_t counter;
        using Vec = fm_vec3_t;
        typedef int32_t Num;
        friend struct Other;

        [[P64::Name("Health")]]
        int32_t health = 100;

        public:
        void someFunction(int32_t incr);
        int32_t get() const { int32_t local = 5; return health + local; }
        template<typename T> T as() const { T t{}; return t; }
        PlayerData() = default;
        PlayerData(const PlayerData&) = delete;
        bool operator==(const PlayerData &o) const;

        protected:
        fm_vec3_t dir = {{1, 2, 3}};
        bool flag = isOk(1);

        private:
        char name[8];
      };
      namespace P64::Script::C1 { P64_DATA_TYPE(PlayerData); }
    )");
    REQUIRE(fieldNames(s) == std::vector<std::string>{"health", "dir", "flag", "name"});
    CHECK(s.fields[1].defaultValue == "1,2,3");
    CHECK(s.fields[3].dataSize == 8);
  }

  TEST_CASE("bit-fields are skipped with a warning")
  {
    TempDir dir{};
    Utils::Logger::clear();
    auto s = parseScript(dir, R"(
      struct PlayerData { uint8_t bits : 3; int32_t a; };
      namespace P64::Script::C1 { P64_DATA_TYPE(PlayerData); }
    )");
    CHECK(fieldNames(s) == std::vector<std::string>{"a"});
    CHECK(logContains("[WRN] Bit-fields"));
  }

  TEST_CASE("base classes are rejected with a warning")
  {
    TempDir dir{};
    Utils::Logger::clear();
    auto s = parseScript(dir, R"(
      struct Base { int32_t a; };
      struct PlayerData : Base { int32_t b; };
      namespace P64::Script::C1 { P64_DATA_TYPE(PlayerData); }
    )");
    CHECK(s.fields.empty());
    CHECK(logContains("has base classes"));
  }

  TEST_CASE("invalid type names give a warning")
  {
    TempDir dir{};
    Utils::Logger::clear();
    auto s = parseScript(dir, "namespace P64::Script::C1 { P64_DATA_TYPE(Foo<int>); }");
    CHECK(s.fields.empty());
    CHECK(logContains("Invalid script data type"));
  }

  TEST_CASE("include cycles terminate")
  {
    TempDir dir{};
    dir.write("src/user/A.h", "#pragma once\n#include \"B.h\"\n");
    dir.write("src/user/B.h", "#pragma once\n#include \"A.h\"\n");
    Utils::Logger::clear();
    auto s = parseScript(dir, R"(
      #include "A.h"
      namespace P64::Script::C1 { P64_DATA_TYPE(Missing); }
    )");
    CHECK(s.fields.empty());
    CHECK(logContains("'Missing' not found"));
  }
}

TEST_SUITE("hasFunction")
{
  TEST_CASE("matches regardless of whitespace")
  {
    CHECK(Utils::CPP::hasFunction("void update(Object& obj)", "void", "update"));
    CHECK(Utils::CPP::hasFunction("void\n  update  (Object& obj)", "void", "update"));
  }

  TEST_CASE("ignores comments and other signatures")
  {
    CHECK_FALSE(Utils::CPP::hasFunction("// void update(Object& obj)", "void", "update"));
    CHECK_FALSE(Utils::CPP::hasFunction("/* void update(Object& obj) */", "void", "update"));
    CHECK_FALSE(Utils::CPP::hasFunction("int update(Object& obj)", "void", "update"));
    CHECK_FALSE(Utils::CPP::hasFunction("void updateAll(Object& obj)", "void", "update"));
  }
}
