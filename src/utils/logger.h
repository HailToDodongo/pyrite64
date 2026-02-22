/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <string>

namespace Utils::Logger
{
  constexpr int LEVEL_INFO = 0;
  constexpr int LEVEL_WARN = 1;
  constexpr int LEVEL_ERROR = 2;

  typedef void (*LogOutputFunc)(const std::string &msg);

  void setOutput(LogOutputFunc outFunc);

  void log(const std::string &msg, int level = LEVEL_INFO);
  void logRaw(const std::string &msg, int level = LEVEL_INFO);
  void clear();

  std::string getLog();
  const std::string& getLogStripped();
}