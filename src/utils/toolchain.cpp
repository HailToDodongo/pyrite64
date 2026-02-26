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

    if(state.mingwPath.empty())return;

    const char* n64InstEnv = std::getenv("N64_INST");
    // If N64_INST is defined in the system, the user probably already
    // has a working toolchain installation so try to use it.
    state.toolchainPath = (n64InstEnv != nullptr)?
      fs::path{n64InstEnv} : state.mingwPath / "pyrite64-sdk";

    state.hasToolchain = fs::exists(state.toolchainPath / "bin" / "mips64-elf-gcc.exe")
                       && fs::exists(state.toolchainPath / "bin" / "mips64-elf-g++.exe");

    state.hasLibdragon = fs::exists(state.toolchainPath / "bin" / "n64tool.exe")
                       && fs::exists(state.toolchainPath / "bin" / "mkdfs.exe")
                       && fs::exists(state.toolchainPath / "include" / "n64.mk");

    state.hasTiny3d = fs::exists(state.toolchainPath / "bin" / "gltf_to_t3d.exe")
                    && fs::exists(state.toolchainPath / "include" / "t3d.mk")
                    && fs::exists(state.toolchainPath / "mips64-elf" / "include" / "t3d");

  #else
    char* n64InstEnv = getenv("N64_INST");
    if(n64InstEnv) {
      state.toolchainPath = fs::path{n64InstEnv};
    }
    if(state.toolchainPath.empty())return;

    state.hasToolchain = fs::exists(state.toolchainPath / "bin" / "mips64-elf-gcc");
    if(!state.hasToolchain)return;

    state.hasLibdragon = fs::exists(state.toolchainPath / "bin" / "n64tool")
                       && fs::exists(state.toolchainPath / "bin" / "mkdfs")
                       && fs::exists(state.toolchainPath / "include" / "n64.mk");
    state.hasTiny3d = fs::exists(state.toolchainPath / "bin" / "gltf_to_t3d")
                    && fs::exists(state.toolchainPath / "include" / "t3d.mk")
                    && fs::exists(state.toolchainPath / "mips64-elf" / "include" / "t3d");
  #endif
}

namespace
{
  void runInstallScript(fs::path mingwPath, bool forceUpdate) {
    // C:\msys64\usr\bin\mintty.exe --hold=error /bin/env MSYSTEM=MINGW64 /bin/bash -l %self_path%mingw_create_env.sh
    auto minttyPath = mingwPath / "usr" / "bin" / "mintty.exe";
    if (!fs::exists(minttyPath)) {
      printf("Error: mintty.exe not found at expected location: %s\n", minttyPath.string().c_str());
      installing.store(false);
      return;
    }

    std::string envVars = "MSYSTEM=MINGW64 ";
    if (forceUpdate) envVars += "FORCE_UPDATE=true ";
    std::string command = minttyPath.string() + " --hold=error /bin/env " + envVars + "/bin/bash -l ";
    
    fs::path scriptPath = Utils::Proc::getAppResourcePath() / "data" / "scripts" / "mingw_create_env.sh";
    command += "\"" + scriptPath.string() + "\"";

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
  bool isInstalled = state.hasToolchain && state.hasLibdragon && state.hasTiny3d;
  std::thread installThread(runInstallScript, state.mingwPath, isInstalled);
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
