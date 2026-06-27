/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "prop.h"

#include "string.h"

namespace PropScope
{
  thread_local std::vector<Layer> stack{};
}

#define ST(v) std::to_string(v)

std::string GenericValue::serialize() const
{
  std::string pre = std::to_string(this->type) + ":";
  switch(type)
  {
    case typeToId<  glm::quat>(): return pre + ST(valQuat.x) + "," + ST(valQuat.y) + "," + ST(valQuat.z) + "," + ST(valQuat.w);
    case typeToId<  glm::vec3>(): return pre + ST(valVec3.x) + "," + ST(valVec3.y) + "," + ST(valVec3.z);
    case typeToId<  glm::vec4>(): return pre + ST(valVec4.x) + "," + ST(valVec4.y) + "," + ST(valVec4.z) + "," + ST(valVec4.w);
    case typeToId< glm::ivec2>(): return pre + ST(valIVec2.x) + "," + ST(valIVec2.y);
    case typeToId<   uint64_t>(): return pre + ST(valU64);
    case typeToId<   uint32_t>(): return pre + ST(valU32);
    case typeToId<    int64_t>(): return pre + ST(valS64);
    case typeToId<    int32_t>(): return pre + ST(valS32);
    case typeToId<      float>(): return pre + ST(valFloat);
    case typeToId<       bool>(): return pre + ST(valBool ? 1 : 0);
    case typeToId<std::string>(): return pre + valString;

    default: return "";
  }
}

void GenericValue::deserialize(const std::string &str)
{
  auto typeMarker = str.find(':');
  if(typeMarker == std::string::npos)return;

  this->type = std::stoi(str.substr(0, typeMarker));
  std::string strVal = str.substr(typeMarker + 1);

  switch(type)
  {
    case typeToId<  glm::quat>():
    {
      auto vals = Utils::splitString(strVal, ',');
      if(vals.size() != 4)break;
      valQuat.x = std::stof(vals[0]);
      valQuat.y = std::stof(vals[1]);
      valQuat.z = std::stof(vals[2]);
      valQuat.w = std::stof(vals[3]);
      break;
    }
    case typeToId<  glm::vec3>():
    {
      auto vals = Utils::splitString(strVal, ',');
      if(vals.size() != 3)break;
      valVec3.x = std::stof(vals[0]);
      valVec3.y = std::stof(vals[1]);
      valVec3.z = std::stof(vals[2]);
      break;
    }
    case typeToId<  glm::vec4>():
    {
      auto vals = Utils::splitString(strVal, ',');
      if(vals.size() != 4)break;
      valVec4.x = std::stof(vals[0]);
      valVec4.y = std::stof(vals[1]);
      valVec4.z = std::stof(vals[2]);
      valVec4.w = std::stof(vals[3]);
      break;
    }
    case typeToId< glm::ivec2>():
    {
      auto vals = Utils::splitString(strVal, ',');
      if(vals.size() != 2)break;
      valIVec2.x = std::stoi(vals[0]);
      valIVec2.y = std::stoi(vals[1]);
      break;
    }
    case typeToId<   uint64_t>(): valU64 = std::stoull(strVal);          break;
    case typeToId<   uint32_t>(): valU32 = (uint32_t)std::stoul(strVal); break;
    case typeToId<    int64_t>(): valS64 = std::stoll(strVal);           break;
    case typeToId<    int32_t>(): valS32 = (int32_t)std::stol(strVal);   break;
    case typeToId<      float>(): valFloat = std::stof(strVal);          break;
    case typeToId<       bool>(): valBool = (std::stoi(strVal) != 0);    break;
    case typeToId<std::string>(): valString = strVal;                    break;

    default:
      type = -1;
    break;
  }
}
