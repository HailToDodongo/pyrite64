/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "cli.h"
#include "argparse/argparse.hpp"
#include "build/projectBuilder.h"
#include "utils/logger.h"

namespace
{
  std::string argProgPath{};
}

const std::string& CLI::getProjectPath()
{
  return argProgPath;
}

CLI::Result CLI::run(int argc, char** argv)
{
  argparse::ArgumentParser prog{"pyrite64", PYRITE_VERSION};
  prog.add_argument("--cli")
   .help("Run in CLI mode (no GUI)")
   .default_value(false)
   .implicit_value(true);

  prog.add_argument("--cmd")
    .help("Command to run")
    .add_choice("build");

  prog.add_argument("project")
    .default_value("")
    .help("Path to project file (.p64proj)")
  ;

  argProgPath = {};
  try {
    prog.parse_args(argc, argv);
  }
  catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << prog;
    return Result::ERROR;
  }

  argProgPath = prog.get<std::string>("project");

  if (prog["--cli"] == false) {
    return Result::GUI;
  }

  auto cmd = prog.get<std::string>("--cmd");

  Utils::Logger::setOutput([](const std::string &msg) {
    fputs(msg.c_str(), stdout);
  });

  printf("Pyrite64 - CLI\n");
  bool res = false;
  if (cmd == "build") {
    printf("Building project: %s\n", argProgPath.c_str());
    res = Build::buildProject(argProgPath);
  }

  return res ? Result::SUCCESS : Result::ERROR;
}
