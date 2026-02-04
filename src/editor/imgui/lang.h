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
    extern const std::string ASSET_NONE_SELECTED;
    extern const std::string ASSET_FILE;
    extern const std::string ASSET_SETTINGS;
    extern const std::string IMAGE_COMBOBOX_FORMAT;
    extern const std::string MODEL_BASE_SCALE;
    extern const std::string MODEL_CREATE_BVH;
    extern const std::string MODEL_COLLISION;
    extern const std::string SCENE_ON_BOOT;
    extern const std::string SCENE_ON_RESET;
  }
}
