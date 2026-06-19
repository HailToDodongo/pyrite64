/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include <string>
#include "imgui.h"

// Registry of value-pin data types and their conversions, populated from data/nodes/_types.js.
namespace Project::Graph::Node
{
  inline constexpr const char* LOGIC_TYPE = "logic";

  struct ValueType
  {
    std::string id{};             // stable id, e.g. "f32"
    std::string name{};           // display name, e.g. "Float"
    std::string cType{"int"};     // generated C++ type, e.g. "float"
    std::string defaultLiteral{"0"}; // C++ literal for a default value
    ImU32 color{IM_COL32(0xFF, 0x99, 0x55, 0xFF)};
    int size{4};                  // storage size (bytes) in a per-instance variable blob
  };

  // creation methods called while reloading specs:
  void clearValueTypes();
  void addValueType(const ValueType &t);
  void addConversion(const std::string &from, const std::string &to, const std::string &tmpl);

  const ValueType* findValueType(const std::string &id);
  bool isLogicType(const std::string &id);

  // Whether a value produced as 'from' may feed an input declared 'to'
  bool canConnect(const std::string &from, const std::string &to);

  // Wraps 'expr' (a value of type 'from') so it reads as type 'to'. 
  // Identity when the types match, applies the registered conversion otherwise.
  std::string convertExpr(const std::string &from, const std::string &to, const std::string &expr);

  // C++ type string for a value type id, "int" when unknown.
  std::string cTypeOf(const std::string &id);
  // Storage size (bytes) of a value type in a per-instance variable blob; 4 if unknown.
  int byteSizeOf(const std::string &id);
  // Pin color for a value type id.
  ImU32 colorOf(const std::string &id);
}
