/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>

namespace Build
{
  struct StringTable
  {
    std::string table{};

    uint32_t insert(const std::string &newString)
    {
      auto strPos = table.find(newString);
      if(strPos == std::string::npos) {
        strPos = table.size();
        table += newString;
        table.push_back('\0');
      }
      return strPos;
    }
  };
}
