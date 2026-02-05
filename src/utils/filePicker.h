/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <functional>
#include <string>

namespace Utils::FilePicker
{
  /**
   * Opens asynchronous file picker dialog.
   * @param cb Callback when the user has selected a file or cancelled the dialog.
   * @param isDirectory If true, opens a directory picker instead of a file picker.
   * @return false if a picker is already open
   */
  bool open(std::function<void(const std::string &path)> cb, bool isDirectory = false, const std::string &title = "");

  /**
   * poll for outstanding file picker results.
   * This must be called from the main thread.
   */
  void poll();
}
