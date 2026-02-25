/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "proc.h"

#include <fstream>
#include <memory>
#include <filesystem>

#include <SDL3/SDL_filesystem.h>

#ifdef _WIN32
  #include <windows.h>
#elif __APPLE__
  #include <mach-o/dyld.h>
  #include <climits>
#else
  #include <unistd.h>
#endif

#include "logger.h"

namespace fs = std::filesystem;

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

fs::path Utils::Proc::getSelfPath()
{
#ifdef _WIN32
  // Windows specific
  wchar_t szPath[MAX_PATH];
  GetModuleFileNameW( NULL, szPath, MAX_PATH );
#elif __APPLE__
  char szPath[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  if (!_NSGetExecutablePath(szPath, &bufsize))
    return fs::path{szPath}.parent_path() / ""; // to finish the folder path with (back)slash
  return {};  // some error
#else
  // Linux specific
  char szPath[PATH_MAX];
  ssize_t count = readlink( "/proc/self/exe", szPath, PATH_MAX );
  if( count < 0 || count >= PATH_MAX )
    return {}; // some error
  szPath[count] = '\0';
#endif

  return fs::path{szPath};
}

fs::path Utils::Proc::getDataRoot()
{
  // Check if the data exist in the executable directory.
  // Check this first because this is where files are during development.
  fs::path execPath = fs::path(SDL_GetBasePath());
  if(fs::exists(execPath / "data") && fs::exists(execPath / "n64")) {
    return execPath;
  }

  // If not found, check if the data exist in the XDG_DATA_HOME directory.
  char* prefDir = SDL_GetPrefPath(nullptr, "pyrite64");
  if (prefDir)
  {
    fs::path dataPath = fs::path(prefDir);
    SDL_free(prefDir);
    if(fs::exists(dataPath / "data") && fs::exists(dataPath / "n64")) {
      return dataPath;
    }
  }

  // Fallback to executable directory, even if it doesn't contain the data.
  // This is to avoid returning empty path which could cause issues.
  return execPath; 
}

fs::path Utils::Proc::getAppDataPath()
{
  char* prefDir = SDL_GetPrefPath("htd", "pyrite64");
  if(prefDir && *prefDir) {
    auto p = fs::path(prefDir);
    SDL_free(prefDir);
    return p;
  }

  printf("Error: SDL_GetPrefPath() failed: %s, fallback to Documents\n", SDL_GetError());
  const char* docsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (docsPath && *docsPath) {
    auto p = fs::path(docsPath) / "pyrite64";
    fs::create_directories(p);
    return p;
  }

  printf("Error: SDL_GetUserFolder() failed: %s, fallback to current directory\n", SDL_GetError());
  return fs::current_path();
}
