/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "logger.h"

#include <atomic>
#include <mutex>

namespace
{
  std::mutex mtx{};
  constinit std::string buff{};
  std::atomic_bool logChanged{false};
  constexpr size_t MAX_BUFF_SIZE = 1024 * 100; // 100kb

  constinit Utils::Logger::LogOutputFunc outputFunc = nullptr;
  constinit std::string logStripped{};
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
  logChanged = true;

  if (outputFunc) {
    outputFunc(buff);
    buff = "";
  }
}

void Utils::Logger::logRaw(const std::string&msg, int level) {
  std::lock_guard lock{mtx};
  buff += msg;
  logChanged = true;

  if (outputFunc) {
    outputFunc(buff);
    buff = "";
  }
}

void Utils::Logger::clear() {
  std::lock_guard lock{mtx};
  buff = "";
  logChanged = true;
}

std::string Utils::Logger::getLog() {
  std::lock_guard lock{mtx};
  if (buff.length() > MAX_BUFF_SIZE) {
    buff = buff.substr(buff.length() - MAX_BUFF_SIZE);
    logChanged = true;
  }
  return buff;
}

const std::string& Utils::Logger::getLogStripped()
{
  if(!logChanged)return logStripped;
  auto log = getLog();

  logStripped = "";

  bool inAnsi = false;
  for(char c : log)
  {
    if(c == '\x1b') {
      inAnsi = true;
    } else if(inAnsi && (c == 'm' || c == 'K')) {
      inAnsi = false;
    } else if(!inAnsi)
    {
      if(c < 32 && c != '\n' && c != '\t') {
        c = '?';
      }
      logStripped += c;
    }
  }
  logChanged = false;
  return logStripped;
}
