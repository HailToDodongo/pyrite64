/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "actions.h"
#include "../utils/logger.h"
#include "imgui/notification.h"

#include <utility>

namespace
{
  Editor::Actions::ActionFn actionCallbacks[0xFF];
}

namespace Editor::Actions
{
  void initGlobalActions();
}

void Editor::Actions::init() {
  for (auto &cb : actionCallbacks) {
    cb = nullptr;
  }
  initGlobalActions();
}

void Editor::Actions::registerAction(Type type, ActionFn fn) {
  actionCallbacks[static_cast<uint8_t>(type)] = std::move(fn);
}

bool Editor::Actions::call(Type type, const std::string &arg) {
  if (actionCallbacks[static_cast<uint8_t>(type)]) {
    try {
      return actionCallbacks[static_cast<uint8_t>(type)](arg);
    } catch (const std::exception &e) {
      Utils::Logger::log("Error executing action: " + std::string(e.what()), Utils::Logger::LEVEL_ERROR);
      Editor::Noti::add(Editor::Noti::Type::ERROR, "Action Failed:\n" + std::string(e.what()));
    }
  }
  return false;
}
