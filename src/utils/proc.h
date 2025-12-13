/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Utils::Proc
{
  std::string runSync(const std::string &cmd);
  bool runSyncLogged(const std::string &cmd);

  std::string getSelfPath();
  std::string getSelfDir();
}