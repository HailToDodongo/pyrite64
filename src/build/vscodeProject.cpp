/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "../utils/string.h"
#include "json.hpp"

namespace
{
  constexpr const char* TASK_LABEL_BUILD = "Pyrite64: Build";
  constexpr const char* TASK_LABEL_CLEAN = "Pyrite64: Clean";

  nlohmann::json makeTask(const std::string &label, const std::string &cmd, const std::string &configPath)
  {
    nlohmann::json task{
      {"label", label},
      {"type", "process"},
      {"command", Utils::Proc::getSelfPath().generic_string()},
      {"args", {"--cli", "--cmd", cmd, configPath}},
      {"options", {{"cwd", "${workspaceFolder}"}}},
      {"problemMatcher", {"$gcc"}},
    };
    if(cmd == "build") {
      task["group"] = {{"kind", "build"}, {"isDefault", true}};
    }
    return task;
  }

  void ensureGitignoreEntry(const fs::path &gitignorePath, const std::string &entry)
  {
    auto content = Utils::FS::loadTextFile(gitignorePath);
    for(auto &line : Utils::splitString(content, '\n')) {
      if(!line.empty() && line.back() == '\r')line.pop_back();
      if(line == entry)return;
    }
    if(!content.empty() && content.back() != '\n')content += "\n";
    content += entry + "\n";
    Utils::FS::saveTextFile(gitignorePath, content);
  }
}

bool Build::generateVSCodeProject(const fs::path &projectPath, const fs::path &configPath, const fs::path &toolchainPath)
{
  if(toolchainPath.empty()) {
    Utils::Logger::log("Cannot generate VSCode project, no toolchain found", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  auto vscodePath = projectPath / ".vscode";
  fs::create_directories(vscodePath);

  auto compilerPath = toolchainPath / "bin" / "mips64-elf-gcc";
  #if defined(_WIN32)
    compilerPath += ".exe";
  #endif

  Utils::FS::saveTextFile(vscodePath / "c_cpp_properties.json", Utils::replaceAll(
    Utils::FS::loadTextFile("data/build/vscode/c_cpp_properties.json"),
    {
      {"{{COMPILER_PATH}}", compilerPath.generic_string()},
      {"{{N64_INST}}",      toolchainPath.generic_string()},
    }
  ));
  Utils::FS::ensureFile(vscodePath / "extensions.json", "data/build/vscode/extensions.json");

  // only replace our own tasks, keep any the user added
  auto tasksPath = vscodePath / "tasks.json";
  nlohmann::json tasksJSON{};
  auto tasksText = Utils::FS::loadTextFile(tasksPath);
  if(!tasksText.empty()) {
    tasksJSON = nlohmann::json::parse(tasksText, nullptr, false, true);
    if(tasksJSON.is_discarded() || !tasksJSON.is_object()) {
      Utils::Logger::log("Failed to parse existing .vscode/tasks.json, not touching it", Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  tasksJSON["version"] = "2.0.0";
  auto tasks = nlohmann::json::array();
  if(tasksJSON["tasks"].is_array()) {
    for(auto &t : tasksJSON["tasks"]) {
      auto label = t.is_object() ? t.value("label", "") : "";
      if(label != TASK_LABEL_BUILD && label != TASK_LABEL_CLEAN)tasks.push_back(t);
    }
  }

  auto absConfigPath = fs::absolute(configPath).generic_string();
  tasks.push_back(makeTask(TASK_LABEL_BUILD, "build", absConfigPath));
  tasks.push_back(makeTask(TASK_LABEL_CLEAN, "clean", absConfigPath));
  tasksJSON["tasks"] = tasks;
  Utils::FS::saveTextFile(tasksPath, tasksJSON.dump(2));

  // contains absolute paths of this machine
  ensureGitignoreEntry(projectPath / ".gitignore", ".vscode");

  Utils::Logger::log("Generated VSCode project in: " + vscodePath.string());
  return true;
}
