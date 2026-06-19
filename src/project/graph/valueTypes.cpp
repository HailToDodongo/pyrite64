/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "valueTypes.h"

#include <unordered_map>
#include <map>

namespace Project::Graph::Node
{
  namespace
  {
    std::unordered_map<std::string, ValueType> g_types{};
    // (from, to) -> conversion template, "{}" marks the source expression.
    std::map<std::pair<std::string, std::string>, std::string> g_conversions{};

    std::string applyTemplate(const std::string &tmpl, const std::string &expr)
    {
      std::string out{};
      for(size_t i = 0; i < tmpl.size(); ) {
        if(tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}') {
          out += expr;
          i += 2;
        } else {
          out.push_back(tmpl[i++]);
        }
      }
      return out;
    }
  }

  void clearValueTypes()
  {
    g_types.clear();
    g_conversions.clear();
  }

  void addValueType(const ValueType &t)
  {
    g_types[t.id] = t;
  }

  void addConversion(const std::string &from, const std::string &to, const std::string &tmpl)
  {
    g_conversions[{from, to}] = tmpl;
  }

  const ValueType* findValueType(const std::string &id)
  {
    auto it = g_types.find(id);
    return it != g_types.end() ? &it->second : nullptr;
  }

  bool isLogicType(const std::string &id)
  {
    return id == LOGIC_TYPE || id.empty();
  }

  bool canConnect(const std::string &from, const std::string &to)
  {
    if(isLogicType(from) || isLogicType(to)) {
      return isLogicType(from) && isLogicType(to);
    }
    if(from == to)return true;
    return g_conversions.find({from, to}) != g_conversions.end();
  }

  std::string convertExpr(const std::string &from, const std::string &to, const std::string &expr)
  {
    if(from == to || isLogicType(from) || isLogicType(to))return expr;
    auto it = g_conversions.find({from, to});
    if(it == g_conversions.end())return expr;
    return applyTemplate(it->second, expr);
  }

  std::string cTypeOf(const std::string &id)
  {
    const auto *t = findValueType(id);
    return t ? t->cType : std::string{"int"};
  }

  int byteSizeOf(const std::string &id)
  {
    const auto *t = findValueType(id);
    return t ? t->size : 4;
  }

  ImU32 colorOf(const std::string &id)
  {
    const auto *t = findValueType(id);
    return t ? t->color : IM_COL32(0xFF, 0x99, 0x55, 0xFF);
  }
}
