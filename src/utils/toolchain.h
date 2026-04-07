/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

namespace Utils
{
  class Toolchain
  {
    public:
      struct State
      {
        fs::path mingwPath{}; // empty if not found, always empty on linux
        fs::path toolchainPath{}; // empty if not found
        bool hasToolchain{};
        bool hasLibdragon{};
        bool hasTiny3d{};

        bool upToDateLibs{}; // false if libdragon / tiny3d are outdated
      };

    private:
      State state{};

    public:
      void scan();

      void install();
      bool isInstalling();

      bool runCmdSyncLogged(const std::string &cmd);

      const State& getState() const { return state; }
  };
}
