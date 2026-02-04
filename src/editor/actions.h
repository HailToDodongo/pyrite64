/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace Editor::Actions
{
  enum class Type : uint8_t
  {
    PROJECT_OPEN,
    PROJECT_CLOSE,
    PROJECT_BUILD,
    PROJECT_CREATE,
    PROJECT_CLEAN,
    ASSETS_RELOAD,
    COPY, PASTE,

    OPEN_NODE_GRAPH
  };

  using ActionFn = std::function<bool(const std::string&)>;

  void init();
  void registerAction(Type type, ActionFn fn);
  bool call(Type type, const std::string &arg = "");
}
