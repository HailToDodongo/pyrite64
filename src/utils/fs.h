/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
namespace fs = std::filesystem;

namespace Utils::FS
{
  inline std::string loadTextFile(const fs::path &path) {
    FILE *file = fopen(path.string().c_str(), "rb");
    if(!file) {
      return "";
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::string content(size, '\0');
    fread(content.data(), 1, size, file);
    fclose(file);

    return content;
  }

  inline bool saveTextFile(const fs::path &path, const std::string &content) {
    FILE *file = fopen(path.string().c_str(), "wb");
    if(!file)return false;
    fwrite(content.data(), 1, content.size(), file);
    fflush(file);
    fclose(file);
    return true;
  }

  std::vector<std::string> scanDirs(const std::string &basePath);

  void ensureFile(const fs::path &path, const fs::path &pathTemplate);

  uint64_t getFileAge(const fs::path &filePath);

  inline std::string toUnixPath(const fs::path &path) {
    auto res = path.string();
    for(auto &c : res) {
      if(c == '\\')c = '/';
    }
    return res;
  }

}
