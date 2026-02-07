/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "logger.h"

#include <mutex>

namespace
{
  std::mutex mtx{};
  constinit std::string buff{};

  constinit Utils::Logger::LogOutputFunc outputFunc = nullptr;
}

void Utils::Logger::setOutput(LogOutputFunc outFunc) {
  outputFunc = outFunc;
}

void Utils::Logger::log(const std::string&msg, int level)
{
  std::lock_guard lock{mtx};
  switch(level) {
    default:
    case LEVEL_INFO:
      buff += "[INF] ";
      break;
    case LEVEL_WARN:
      buff += "[WRN] ";
      break;
    case LEVEL_ERROR:
      buff += "[ERR] ";
      break;
  }
  buff += msg + "\n";

  if (outputFunc) {
    outputFunc(buff);
    buff = "";
  }
}

void Utils::Logger::logRaw(const std::string&msg, int level) {
  std::lock_guard lock{mtx};
  buff += msg;

  if (outputFunc) {
    outputFunc(buff);
    buff = "";
  }
}

void Utils::Logger::clear() {
  std::lock_guard lock{mtx};
  buff = "";
}

std::string Utils::Logger::getLog() {
  std::lock_guard lock{mtx};
  return buff;
}
