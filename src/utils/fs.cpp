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

void Utils::FS::ensureDir(const std::filesystem::path &path)
{
  if(!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path);
  }
}

void Utils::FS::ensureFile(const std::filesystem::path &path, const std::filesystem::path &pathTemplate)
{
  if(!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::copy_file(pathTemplate, path);
  }
}

void Utils::FS::copyDir(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath)
{
    try {
        // Ensure parent directories exist
        std::filesystem::create_directories(dstPath);

        // Copy template project
        std::filesystem::copy( srcPath, dstPath, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing );

    } catch (const std::filesystem::filesystem_error& e) {
        // Log the error instead of crashing
        printf("Filesystem error: %s\n", e.what());
    }
}

void Utils::FS::delFile(const std::string &filePath)
{
  std::filesystem::remove(filePath);
}

void Utils::FS::delDir(const std::string &dirPath)
{
  std::filesystem::remove_all(dirPath);
}

uint64_t Utils::FS::getFileAge(const std::filesystem::path &filePath)
{
  if(!std::filesystem::exists(filePath)) {
    return 0;
  }
  auto ftime = std::filesystem::last_write_time(filePath);
  return ftime.time_since_epoch().count();
}
