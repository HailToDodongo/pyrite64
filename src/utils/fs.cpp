/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "fs.h"
#include <filesystem>

std::vector<std::string> Utils::FS::scanDirs(const std::string &basePath)
{
  std::vector<std::string> dirs{};
  for (const auto &entry : fs::recursive_directory_iterator(basePath))
  {
    if (entry.is_directory()) {
      auto relPath = fs::relative(entry.path(), basePath).string();
      dirs.push_back(relPath);
    }
  }
  return dirs;
}

void Utils::FS::ensureFile(const fs::path &path, const fs::path &pathTemplate)
{
  if(!fs::exists(path)) {
    fs::create_directories(path.parent_path());
    fs::copy_file(pathTemplate, path);
  }
}

uint64_t Utils::FS::getFileAge(const fs::path &filePath)
{
  if(!fs::exists(filePath)) {
    return 0;
  }
  auto ftime = fs::last_write_time(filePath);
  return ftime.time_since_epoch().count();
}

void Utils::FS::delTypeRecursive(const fs::path &basePath, const std::string &fileExt)
{
  for (const auto &entry : fs::recursive_directory_iterator(basePath))
  {
    if (entry.is_regular_file() && entry.path().extension() == fileExt) {
      fs::remove(entry.path());
    }
  }
}
