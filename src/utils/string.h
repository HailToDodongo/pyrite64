/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Utils
{
  inline std::string replaceFirst(const std::string &str, const std::string &search, const std::string &repalce) {
    std::string result = str;
    size_t pos = result.find(search);
    if(pos != std::string::npos) {
      result.replace(pos, search.length(), repalce);
    }
    return result;
  }

  inline std::string replaceAll(const std::string &str, const std::string &search, const std::string &repalce) {
    std::string result = str;
    size_t pos = 0;
    while((pos = result.find(search, pos)) != std::string::npos) {
      result.replace(pos, search.length(), repalce);
      pos += repalce.length();
    }
    return result;
  }

  inline std::string padLeft(const std::string &str, char padChar, size_t totalLength) {
    if(str.length() >= totalLength)return str;
    return std::string(totalLength - str.length(), padChar) + str;
  }

  inline std::string join(const std::vector<std::string> &values, const std::string &separator) {
    std::string result{};
    for(size_t i=0; i<values.size(); ++i) {
      result += values[i];
      if(i < values.size()-1) {
        result += separator;
      }
    }
    return result;
  }
}