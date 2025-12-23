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

  inline std::string replaceAll(std::string str, const std::vector<std::pair<std::string, std::string>> &replacements)
  {
    for (const auto& [search, replace] : replacements) {
      str = replaceAll(str, search, replace);
    }
    return str;
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

  inline std::vector<std::string> splitString(const std::string &str, char delimiter) {
    std::vector<std::string> result{};
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string::npos) {
      result.push_back(str.substr(start, end - start));
      start = end + 1;
      end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
  }

  inline uint64_t parseU64(const std::string &str)
  {
    try {
      return std::stoul(str);
    } catch (...) {
      return 0;
    }
  }
}