/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "proc.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#ifdef _WIN32
  #include <windows.h>
#elif __APPLE__
  #include <mach-o/dyld.h>
#else
  #include <unistd.h>
#endif

#include "logger.h"

namespace fs = std::filesystem;

namespace
{
  constexpr uint32_t BUFF_SIZE = 128;

#if defined(__linux__)
  bool isWSL()
  {
    return std::getenv("WSL_INTEROP") != nullptr || std::getenv("WSL_DISTRO_NAME") != nullptr;
  }

  std::optional<std::string> wslToWindowsPath(const std::string &path)
  {
    std::error_code absEc;
    std::string linuxPath = fs::absolute(fs::path(path), absEc).generic_string();
    if (absEc) linuxPath = fs::path(path).generic_string();

    std::string escapedPath;
    escapedPath.reserve(linuxPath.size());
    for (char c : linuxPath) {
      if (c == '\\' || c == '"') escapedPath.push_back('\\');
      escapedPath.push_back(c);
    }

    std::string command = "wslpath -w -- \"" + escapedPath + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return std::nullopt;

    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      output += buffer.data();
    }

    if (pclose(pipe) != 0) return std::nullopt;

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
      output.pop_back();
    }

    return output.empty() ? std::nullopt : std::optional<std::string>{output};
  }
#endif
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
  // First call with a null buffer reports the size needed (including the null terminator).
  uint32_t bufsize = 0;
  _NSGetExecutablePath(nullptr, &bufsize);
  std::string szPath(bufsize, '\0');
  if (_NSGetExecutablePath(szPath.data(), &bufsize) != 0)
    return {}; // some error
  return fs::path{szPath.c_str()}.parent_path() / ""; // finish the folder path with a slash
#else
  // Linux specific. readlink does not null-terminate and gives no length up front,
  // so grow the buffer until the path fits.
  std::string szPath(256, '\0');
  while (true) {
    ssize_t count = readlink("/proc/self/exe", szPath.data(), szPath.size());
    if (count < 0) return {}; // some error
    if (static_cast<size_t>(count) < szPath.size()) {
      szPath.resize(count);
      break;
    }
    szPath.resize(szPath.size() * 2); // truncated, retry with a larger buffer
  }
#endif

  return fs::path{szPath};
}

fs::path Utils::Proc::getAppResourcePath()
{
  // Check if the data exist in the executable directory.
  // Check this first because this is where files are during development.
  fs::path execPath = fs::path(SDL_GetBasePath());
  if(fs::exists(execPath / "data") && fs::exists(execPath / "n64")) {
    return execPath;
  }

  // If not found, check if the data exist in the XDG_DATA_HOME directory.
  char* prefDir = SDL_GetPrefPath(PYRITE_ORG_NAME, PYRITE_APP_NAME);
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
  // On Linux, follow the XDG Base Directory Specification and store application data in XDG_CONFIG_HOME. On other platforms, use SDL_GetPrefPath() which gives a suitable directory for storing application data.
  #ifndef _WIN32
    const char* envr = SDL_getenv("XDG_CONFIG_HOME");
    if (envr && *envr) {
      auto p = fs::path(envr) / PYRITE_ORG_NAME / PYRITE_APP_NAME;
      fs::create_directories(p);
      return p;
    }
    // Fallback to "~/.config/<org>/<app>" if XDG_CONFIG_HOME is not set to conform to the spec.
    else {
      const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
      if (home && *home) {
        auto p = fs::path(home) / ".config" / PYRITE_ORG_NAME / PYRITE_APP_NAME;
        fs::create_directories(p);
        return p;
      }
    }
  #endif

  char* prefDir = SDL_GetPrefPath(PYRITE_ORG_NAME, PYRITE_APP_NAME);
  if(prefDir && *prefDir) {
    auto p = fs::path(prefDir);
    SDL_free(prefDir);
    fs::create_directories(p);
    return p;
  }

  printf("Error: SDL_GetPrefPath() failed: %s, fallback to Documents\n", SDL_GetError());
  const char* docsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (docsPath && *docsPath) {
    auto p = fs::path(docsPath) / PYRITE_ORG_NAME / PYRITE_APP_NAME;
    fs::create_directories(p);
    return p;
  }

  printf("Error: SDL_GetUserFolder() failed: %s, fallback to current directory\n", SDL_GetError());
  return fs::current_path();
}

fs::path Utils::Proc::getProjectsPath()
{
  const char* docsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (docsPath && *docsPath) {
    return fs::path(docsPath) / PYRITE_APP_NAME;
  }

  const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
  if (home && *home) {
    return fs::path(home) / PYRITE_APP_NAME / "projects";
  }

  return fs::current_path() / "projects";
}

bool Utils::Proc::openFile(const std::string &path)
{
#if defined(__linux__)
  if (isWSL()) {
    const auto windowsPath = wslToWindowsPath(path);
    if (windowsPath) {
      const char* args[] = { "cmd.exe", "/C", "start", "", windowsPath->c_str(), NULL };
      SDL_CreateProcess(args, false);
      return true;
    }
    SDL_OpenURL(path.c_str());
    return false;
  }
#endif
  SDL_OpenURL(path.c_str());
  return true;
}

bool Utils::Proc::openInFileBrowser(const std::string &path)
{
#if defined(_WIN32)
  std::string explorerArgs = "/select," + path;
  const char* args[] = { "explorer.exe", explorerArgs.c_str(), NULL };
  SDL_CreateProcess(args, false);
  return true;
#elif defined(__APPLE__)
  const char* args[] = { "open", "-R", path.c_str(), NULL };
  SDL_CreateProcess(args, false);
  return true;
#else
#if defined(__linux__)
  if (isWSL()) {
    const auto windowsPath = wslToWindowsPath(path);
    if (windowsPath) {
      std::string explorerArgs = std::string("/select,") + *windowsPath;
      const char* args[] = { "explorer.exe", explorerArgs.c_str(), NULL };
      SDL_CreateProcess(args, false);
      return true;
    }
    return false;
  }
#endif
  std::string pathArg = std::format("array:string:file:///{}", path);
  const char* args[] = {
    "dbus-send",
    "--session",
    "--dest=org.freedesktop.FileManager1",
    "--type=method_call",
    "/org/freedesktop/FileManager1",
    "org.freedesktop.FileManager1.ShowItems",
    pathArg.c_str(),
    "string:",
    NULL
  };
  SDL_CreateProcess(args, false);
  return true;
#endif
}

void Utils::Proc::openURL(const std::string &url)
{
  SDL_OpenURL(url.c_str());
}
