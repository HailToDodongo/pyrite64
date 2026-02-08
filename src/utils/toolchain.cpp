/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "toolchain.h"
#include "logger.h"
#include "proc.h"
#include <filesystem>
#include <atomic>
#include <thread>

namespace
{
  std::atomic_bool installing{false};
}

void Utils::Toolchain::scan()
{
  //printf("Scanning for toolchain...\n");
  state = {};
  #if defined(_WIN32)

    // Scan "C:\" directories for anything containing "msys"
    state.mingwPath = std::filesystem::path{"C:\\msys64"};
    if(!std::filesystem::exists(state.mingwPath)) {
      state.mingwPath.clear();
      return;
    }

    //printf("Mingw Path: %s\n", state.mingwPath.string().c_str());
    if(state.mingwPath.empty())return;

    auto toolPath = state.mingwPath / "pyrite64-sdk";
    if(std::filesystem::exists(toolPath / "bin" / "mips64-elf-gcc.exe")
     && std::filesystem::exists(toolPath / "bin" / "mips64-elf-g++.exe")) {
      state.hasToolchain = true;
    }

    if(std::filesystem::exists(toolPath / "bin" / "n64tool.exe")
     && std::filesystem::exists(toolPath / "bin" / "mkdfs.exe")
     && std::filesystem::exists(toolPath / "include" / "n64.mk")
    ) {
      state.hasLibdragon = true;
    }

    if(std::filesystem::exists(toolPath / "bin" / "gltf_to_t3d.exe")
     && std::filesystem::exists(toolPath / "include" / "t3d.mk")
     && std::filesystem::exists(toolPath / "mips64-elf" / "include" / "t3d")
    ) {
      state.hasTiny3d = true;
    }
    

  #else
    // @TODO:
    state.hasToolchain = true;
    state.hasLibdragon = true;
    state.hasTiny3d = true;
  #endif
}

namespace
{
  void runInstallScript(std::filesystem::path mingwPath) {
    // C:\msys64\usr\bin\mintty.exe /bin/env MSYSTEM=MINGW64 /bin/bash -l %self_path%mingw_create_env.sh
    auto minttyPath = mingwPath / "usr" / "bin" / "mintty.exe";
    if (!std::filesystem::exists(minttyPath)) {
      printf("Error: mintty.exe not found at expected location: %s\n", minttyPath.string().c_str());
      installing.store(false);
      return;
    }

    std::string command = minttyPath.string() + " /bin/env MSYSTEM=MINGW64 /bin/bash -l ";
    
    std::filesystem::path selfPath{Utils::Proc::getSelfPath()};
    selfPath = selfPath.parent_path(); // remove executable name
    selfPath = selfPath / "data" / "scripts" / "mingw_create_env.sh"; // add script path
    command += "\"" + selfPath.string() + "\"";

    auto res = Utils::Proc::runSync(command);
    printf("Res: %s : %s\n", command.c_str(), res.c_str());
    installing.store(false);
  }
}

void Utils::Toolchain::install()
{
  if (installing.load()) {
    printf("Toolchain installation already in progress.\n");
    return;
  }

  installing.store(true);
  std::thread installThread(runInstallScript, state.mingwPath);
  installThread.detach();
}

bool Utils::Toolchain::isInstalling()
{
  return installing.load();
}

bool Utils::Toolchain::runCmdSyncLogged(const std::string &cmd)
{
  #if defined(_WIN32)
    auto minttyPath = state.mingwPath / "usr" / "bin" / "bash.exe";
    //std::string command = minttyPath.string() + " --log - -w hide /bin/env MSYSTEM=MINGW64 " + cmd;
    std::string command = minttyPath.string() + " -lc \"" + cmd + "\"";
    //std::string command = cmd;
    for(char &c : command) {
      if(c == '\\')c = '/';
    }
    return Utils::Proc::runSyncLogged(command);
    //Utils::Logger::logRaw(run_bash(command));
    //return true;
    
  #else
    return Utils::Proc::runSyncLogged(cmd);
  #endif
}
