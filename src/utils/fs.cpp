/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "fs.h"
#include <filesystem>

std::vector<std::string> Utils::FS::scanDirs(const std::string &basePath)
{
  std::vector<std::string> dirs{};
  for (const auto &entry : std::filesystem::recursive_directory_iterator(basePath))
  {
    if (entry.is_directory()) {
      auto relPath = std::filesystem::relative(entry.path(), basePath).string();
      dirs.push_back(relPath);
    }
  }
  return dirs;
}

void Utils::FS::ensureDir(const std::string &path)
{
  std::filesystem::path fsPath{path};
  if(!std::filesystem::exists(fsPath)) {
    std::filesystem::create_directories(fsPath);
  }
}

void Utils::FS::ensureFile(const std::string &path, const std::string &pathTemplate)
{
  std::filesystem::path fsPath{path};
  if(!std::filesystem::exists(fsPath)) {
    std::filesystem::create_directories(fsPath.parent_path());
    std::filesystem::copy_file(pathTemplate, fsPath);
  }
}

void Utils::FS::copyDir(const std::string &srcPath, const std::string &dstPath)
{
  std::filesystem::path fsSrcPath{srcPath};
  std::filesystem::path fsDstPath{dstPath};
  std::filesystem::copy(fsSrcPath, fsDstPath, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
}

void Utils::FS::delFile(const std::string &filePath)
{
  std::filesystem::remove(filePath);
}

void Utils::FS::delDir(const std::string &dirPath)
{
  std::filesystem::remove_all(dirPath);
}

uint64_t Utils::FS::getFileAge(const std::string &filePath)
{
  std::filesystem::path fsPath{filePath};
  if(!std::filesystem::exists(fsPath)) {
    return 0;
  }
  auto ftime = std::filesystem::last_write_time(fsPath);
  return ftime.time_since_epoch().count();
}
