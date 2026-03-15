/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Utils::FilePicker
{
  struct Options
  {
    struct Filter
    {
      const char *name;
      const char *pattern;
    };

    std::string title{};
    bool isDirectory{false};
    std::vector<Filter> customFilters{};
  };

  /**
   * Opens asynchronous file picker dialog.
   * @param cb Callback when the user has selected a file or cancelled the dialog.
   * @return false if a picker is already open
   */
  bool open(std::function<void(const std::string &path)> cb, const Options &options);

  /**
   * poll for outstanding file picker results.
   * This must be called from the main thread.
   */
  void poll();
}
