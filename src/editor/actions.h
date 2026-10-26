/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Editor::Actions
{
  namespace Type
  {
    constexpr const char* PROJECT_OPEN       = "p64.project.open";
    constexpr const char* PROJECT_CLOSE      = "p64.project.close";
    constexpr const char* PROJECT_BUILD      = "p64.project.build";
    constexpr const char* PROJECT_BUILD_RUN  = "p64.project.buildRun";
    constexpr const char* PROJECT_CREATE     = "p64.project.create";
    constexpr const char* PROJECT_CLEAN      = "p64.project.clean";
    constexpr const char* PROJECT_GEN_VSCODE = "p64.project.genVSCode";
    constexpr const char* ASSETS_RELOAD      = "p64.assets.reload";
    constexpr const char* COPY               = "p64.edit.copy";
    constexpr const char* PASTE              = "p64.edit.paste";
    constexpr const char* OPEN_NODE_GRAPH    = "p64.nodeGraph.open";
  }

  using ActionFn = std::function<bool(const std::string&)>;

  struct Action
  {
    std::string id{};
    std::string name{};
    ActionFn fn{};
    std::function<bool()> isAvailable{};
    bool showInSearch{true};
  };

  void init();

  /**
   * Adds or replaces an action, the returned reference can be used to set further options.
   */
  Action& registerAction(const std::string &id, const std::string &name, ActionFn fn);
  void unregisterAction(const std::string &id);

  bool call(const std::string &id, const std::string &arg = "");

  [[nodiscard]] bool isAvailable(const Action &action);
  [[nodiscard]] std::vector<const Action*> getActions();
}
