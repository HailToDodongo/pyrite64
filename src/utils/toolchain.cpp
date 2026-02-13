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
    state.mingwPath = fs::path{"C:\\msys64"};
    if(!fs::exists(state.mingwPath)) {
      state.mingwPath.clear();
      return;
    }

    //printf("Mingw Path: %s\n", state.mingwPath.string().c_str());
    if(state.mingwPath.empty())return;

    auto toolPath = state.mingwPath / "pyrite64-sdk";
    if(fs::exists(toolPath / "bin" / "mips64-elf-gcc.exe")
     && fs::exists(toolPath / "bin" / "mips64-elf-g++.exe")) {
      state.hasToolchain = true;
    }

    if(fs::exists(toolPath / "bin" / "n64tool.exe")
     && fs::exists(toolPath / "bin" / "mkdfs.exe")
     && fs::exists(toolPath / "include" / "n64.mk")
    ) {
      state.hasLibdragon = true;
    }

    if(fs::exists(toolPath / "bin" / "gltf_to_t3d.exe")
     && fs::exists(toolPath / "include" / "t3d.mk")
     && fs::exists(toolPath / "mips64-elf" / "include" / "t3d")
    ) {
      state.hasTiny3d = true;
    }
    

  #else
    char* n64InstEnv = getenv("N64_INST");
    if(n64InstEnv) {
      state.mingwPath = fs::path{n64InstEnv};
    }
    if(state.mingwPath.empty())return;

    state.hasToolchain = fs::exists(state.mingwPath / "bin" / "mips64-elf-gcc");
    if(!state.hasToolchain)return;

    fs::path n64Path{n64InstEnv};
    state.hasLibdragon = fs::exists(n64Path / "bin" / "n64tool")
                       && fs::exists(n64Path / "bin" / "mkdfs")
                       && fs::exists(n64Path / "include" / "n64.mk");
    state.hasTiny3d = fs::exists(n64Path / "bin" / "gltf_to_t3d")
                    && fs::exists(n64Path / "include" / "t3d.mk")
                    && fs::exists(n64Path / "mips64-elf" / "include" / "t3d");
  #endif
}

namespace
{
  void runInstallScript(fs::path mingwPath) {
    // C:\msys64\usr\bin\mintty.exe /bin/env MSYSTEM=MINGW64 /bin/bash -l %self_path%mingw_create_env.sh
    auto minttyPath = mingwPath / "usr" / "bin" / "mintty.exe";
    if (!fs::exists(minttyPath)) {
      printf("Error: mintty.exe not found at expected location: %s\n", minttyPath.string().c_str());
      installing.store(false);
      return;
    }

    std::string command = minttyPath.string() + " /bin/env MSYSTEM=MINGW64 /bin/bash -l ";
    
    fs::path selfPath{Utils::Proc::getSelfPath()};
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
    std::string command = minttyPath.string() + " -lc '" + cmd + "'";
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
