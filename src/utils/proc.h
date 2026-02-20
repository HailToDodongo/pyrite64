/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace Utils::Proc
{
  std::string runSync(const std::string &cmd);
  bool runSyncLogged(const std::string &cmd);

  /// @brief 
  /// @return Return path to itself, i.e. the executable path.
  fs::path getSelfPath();

  /// @brief 
  /// @return Return path to the directory where data files are stored. This can be the executable directory (on Windows or during development), or the XDG_DATA_HOME directory on an installed Linux build.
  fs::path getDataRoot();
}