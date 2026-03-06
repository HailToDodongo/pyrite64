/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Utils::Updater
{
  /**
   * Checks if a newer version is avaialble and returns it.
   * If none is available, an empty string is returned.
   * @return version, e.g. "v1.2.3" or ""
   */
  std::string getNewerVersion();

  /**
   * Starts an update, this currently only opens a download page.
   * @param targetVersion The version to update to, e.g. "v1.2.3"
   */
  void doUpdate(const std::string &targetVersion);
}
