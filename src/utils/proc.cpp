/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "proc.h"

#include <fstream>
#include <memory>
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#elif __APPLE__
  #include <mach-o/dyld.h>
  #include <climits>
#else
  #include <unistd.h>
#endif

#include "logger.h"

namespace
{
  constexpr uint32_t BUFF_SIZE = 128;
}

std::string Utils::Proc::runSync(const std::string &cmd)
{
  std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if(!pipe)return "";

  char buffer[BUFF_SIZE];
  std::string result{};

  while(!feof(pipe.get()))
  {
    if(fgets(buffer, BUFF_SIZE, pipe.get()) != nullptr) {
      result += buffer;
    }
  }
  return result;
}

bool Utils::Proc::runSyncLogged(const std::string&cmd) {
  auto cmdWithErr = cmd + " 2>&1"; // @TODO: windows handling
  FILE* pipe = popen(cmdWithErr.c_str(), "r");
  if(!pipe)return "";

  char buffer[BUFF_SIZE];
  while(!feof(pipe))
  {
    if(fgets(buffer, BUFF_SIZE, pipe) != nullptr) {
      Logger::logRaw(buffer);
    }
  }
  return pclose(pipe) == 0;
}

std::string Utils::Proc::getSelfPath()
{
#ifdef _WIN32
  // Windows specific
  wchar_t szPath[MAX_PATH];
  GetModuleFileNameW( NULL, szPath, MAX_PATH );
#elif __APPLE__
  char szPath[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  if (!_NSGetExecutablePath(szPath, &bufsize))
    return std::filesystem::path{szPath}.parent_path() / ""; // to finish the folder path with (back)slash
  return {};  // some error
#else
  // Linux specific
  char szPath[PATH_MAX];
  ssize_t count = readlink( "/proc/self/exe", szPath, PATH_MAX );
  if( count < 0 || count >= PATH_MAX )
    return {}; // some error
  szPath[count] = '\0';
#endif

  return std::filesystem::path{szPath}; // to finish the folder path with (back)slash
}

std::string Utils::Proc::getSelfDir()
{
  return std::filesystem::path{getSelfPath()}.parent_path() / "";
}
