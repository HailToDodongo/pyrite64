/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

struct JSRuntime;
struct JSContext;

namespace Project { class Project; }

namespace Editor::ScriptHost
{
  struct Context;
  
  struct EvalResult
  {
    std::string text{};
    bool success{};
  };

  bool start(Project::Project &project);
  void stop();

  [[nodiscard]] bool isRunning();

  /**
   * Evaluates code in the global editor scope.
   * Result is JSON as a string, or some raw text for exceptions.
   */
  EvalResult eval(const std::string &code, const std::string &fileName = "<shell>");

  /**
   * Runs code from a file, otherwise save as eval()
   */
  EvalResult runFile(const std::string &path);

  const Context& getContext();
}
