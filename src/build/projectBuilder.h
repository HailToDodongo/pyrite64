/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "sceneContext.h"
#include "../project/project.h"

namespace Build
{
  void buildScene(Project::Project &project, const Project::SceneEntry &scene, SceneCtx &ctx);
  void buildScripts(Project::Project &project, SceneCtx &sceneCtx);
  void buildGlobalScripts(Project::Project &project, SceneCtx &sceneCtx);

  bool buildProject(std::string path);
}
