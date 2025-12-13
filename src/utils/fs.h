/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>

namespace Utils::FS
{
  inline std::string loadTextFile(const std::string &path) {
    FILE *file = fopen(path.c_str(), "rb");
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

  inline void saveTextFile(const std::string &path, const std::string &content) {
    FILE *file = fopen(path.c_str(), "w");
    if(!file)return;
    fwrite(content.data(), 1, content.size(), file);
    fclose(file);
  }

  std::vector<std::string> scanDirs(const std::string &basePath);

  void ensureDir(const std::string &path);
  void ensureFile(const std::string &path, const std::string &pathTemplate);

  void delFile(const std::string &filePath);
}
