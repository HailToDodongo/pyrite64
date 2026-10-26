/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "actions.h"
#include "../utils/logger.h"
#include "imgui/notification.h"

#include <unordered_map>
#include <utility>

namespace
{
  std::unordered_map<std::string, Editor::Actions::Action> actions{};
}

namespace Editor::Actions
{
  void initGlobalActions();
}

void Editor::Actions::init() {
  actions.clear();
  initGlobalActions();
}

Editor::Actions::Action& Editor::Actions::registerAction(const std::string &id, const std::string &name, ActionFn fn) {
  auto &action = actions[id];
  action = {.id = id, .name = name, .fn = std::move(fn)};
  return action;
}

void Editor::Actions::unregisterAction(const std::string &id) {
  actions.erase(id);
}

bool Editor::Actions::isAvailable(const Action &action) {
  return action.fn && (!action.isAvailable || action.isAvailable());
}

std::vector<const Editor::Actions::Action*> Editor::Actions::getActions() {
  std::vector<const Action*> res{};
  res.reserve(actions.size());
  for(auto &[id, action] : actions)res.push_back(&action);
  return res;
}

bool Editor::Actions::call(const std::string &id, const std::string &arg) {
  auto it = actions.find(id);
  if(it == actions.end()) {
    Utils::Logger::log("Unknown action: " + id, Utils::Logger::LEVEL_WARN);
    return false;
  }
  if(!isAvailable(it->second))return false;

  try {
    return it->second.fn(arg);
  } catch (const std::exception &e) {
    Utils::Logger::log("Error executing action: " + std::string(e.what()), Utils::Logger::LEVEL_ERROR);
    Editor::Noti::add(Editor::Noti::Type::ERROR, "Action Failed:\n" + std::string(e.what()));
  }
  return false;
}
