/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <source_location>
#include <string>

namespace P64::Log
{
  using srcLoc = std::source_location;

  #define PREFIX_ERROR "\033[31;7m" "[ERR]"
  #define PREFIX_WARN  "\033[33;7m" "[WRN]"
  #define PREFIX_INFO               "[INF]"

  #define RESET_SUFFIX "\033[0m\n"

  #ifndef SRC_ROOT_PATH_LENGTH
    #define SRC_ROOT_PATH_LENGTH 0
  #endif

  template <typename... ARGS>
  void log(const char* str, const srcLoc loc, const char* const prefix,  ARGS &&... args)
  {
    auto fmt = std::string(prefix) + " [%s:%d] " + str + RESET_SUFFIX;
    debugf(fmt.c_str(), loc.file_name() + SRC_ROOT_PATH_LENGTH, loc.line(), args...);
  }

  template <typename... ARGS>
  struct info { info(const char* str, ARGS&&... args, srcLoc loc = srcLoc::current()) {
      log(str, loc, PREFIX_INFO, args...);
  }};

  template <typename... ARGS>
  struct warn { warn(const char* str, ARGS&&... args, srcLoc loc = srcLoc::current()) {
      log(str, loc, PREFIX_WARN, args...);
  }};

  template <typename... ARGS>
  struct error { error(const char* str, ARGS&&... args, srcLoc loc = srcLoc::current()) {
      log(str, loc, PREFIX_ERROR, args...);
  }};

  template <typename... Ts> info(const char* str, Ts&&...args) -> info<Ts...>;
  template <typename... Ts> warn(const char* str, Ts&&...args) -> warn<Ts...>;
  template <typename... Ts> error(const char* str, Ts&&...args) -> error<Ts...>;
}