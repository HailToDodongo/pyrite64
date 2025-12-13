/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "cli.h"
#include "argparse/argparse.hpp"
#include "build/projectBuilder.h"
#include "utils/logger.h"

CLI::Result CLI::run(int argc, char** argv)
{
  argparse::ArgumentParser prog{"pyrite64", "1.0.0"};
  prog.add_argument("--cli")
   .help("Run in CLI mode (no GUI)")
   .default_value(false)
   .implicit_value(true);

  prog.add_argument("--project")
    .help("Path to project directory")
    //.required()
    ;

  prog.add_argument("--cmd")
    .help("Command to run")
    //.required()
    .add_choice("build");

  try {
    prog.parse_args(argc, argv);
  }
  catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << prog;
    return Result::ERROR;
  }

  if (prog["--cli"] == false) {
    return Result::GUI;
  }

  auto cmd = prog.get<std::string>("--cmd");
  auto projPath = prog.get<std::string>("--project");

  Utils::Logger::setOutput([](const std::string &msg) {printf(msg.c_str());});

  printf("Pyrite64 - CLI\n");
  bool res = false;
  if (cmd == "build") {
    printf("Building project: %s\n", projPath.c_str());
    res = Build::buildProject(projPath);
  }

  return res ? Result::SUCCESS : Result::ERROR;
}
