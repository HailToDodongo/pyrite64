/**
* @copyright 2026 - Nolan Baker
* @license MIT
*/
#pragma once

#include <vector>
#include <string>

namespace Editor::RecentProjects {
  extern std::vector<std::string> recentPaths;

  std::string getJsonPath();
  std::string getMostRecentPath();
  void setMostRecentPath(const std::string &path);
}
