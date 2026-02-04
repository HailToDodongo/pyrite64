/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Editor
{

  void applyLang(const std::string &lang);
  const char* message(const std::string &id);

  namespace Message {
    extern const std::string NO_ASSET_SELECTED;
    extern const std::string ASSET_FILE;
    extern const std::string ON_BOOT;
    extern const std::string ON_RESET;
  }
}
