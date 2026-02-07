/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace Utils::FS
{
  inline std::string loadTextFile(const std::filesystem::path &path) {
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

  inline void saveTextFile(const std::filesystem::path &path, const std::string &content) {
    FILE *file = fopen(path.string().c_str(), "w");
    if(!file)return;
    fwrite(content.data(), 1, content.size(), file);
    fclose(file);
  }

  std::vector<std::string> scanDirs(const std::string &basePath);

  void ensureDir(const std::filesystem::path &path);
  void ensureFile(const std::filesystem::path &path, const std::filesystem::path &pathTemplate);
  void copyDir(const std::filesystem::path &srcPath, const std::filesystem::path &dstPath);

  void delFile(const std::string &filePath);
  void delDir(const std::string &dirPath);

  uint64_t getFileAge(const std::filesystem::path &filePath);

}
