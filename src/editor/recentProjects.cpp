/**
* @copyright 2026 - Nolan Baker
* @license MIT
*/

#include "recentProjects.h"
#include <filesystem>
#include "json.hpp"
#include "../utils/fs.h"
#include "../utils/proc.h"
#include "../utils/json.h"

namespace Editor::RecentProjects {
  std::vector<std::string> recentPaths = {};

  std::string getJsonPath() {
    auto path = Utils::Proc::getAppDataPath() / "recent.json";
    return path.string();
  }

  std::string getMostRecentPath() {
    if (!recentPaths.empty()) return recentPaths.front();
    
    nlohmann::json json;
    try {
      json = Utils::JSON::loadFile(getJsonPath());
      if (!json.is_array()) return "";
      recentPaths = json.get<std::vector<std::string>>();
    } catch (const std::exception& e) {
      fprintf(stderr, "Error loading recent.json: %s\n", e.what());
      return "";
    }

    if (recentPaths.empty()) return "";
    return recentPaths.front();
  }

  void setMostRecentPath(const std::string &path) {
    recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), path), recentPaths.end());
    recentPaths.insert(recentPaths.begin(), path);

    try {
      nlohmann::json json = recentPaths;
      Utils::FS::saveTextFile(getJsonPath(), json.dump(2));
    } catch (const std::exception& e) {
      fprintf(stderr, "Error saving recent.json: %s\n", e.what());
    }
  }
}
