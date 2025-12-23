/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "sceneContext.h"
#include "../project/project.h"

namespace Build
{
  // helper
  bool assetBuildNeeded(const Project::AssetManager::Entry &asset, const std::string &outPath);

  // Asset builds
  void buildScene(Project::Project &project, const Project::SceneEntry &scene, SceneCtx &ctx);
  void buildScripts(Project::Project &project, SceneCtx &sceneCtx);
  void buildGlobalScripts(Project::Project &project, SceneCtx &sceneCtx);

  bool buildT3DMAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildFontAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildTextureAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildAudioAssets(Project::Project &project, SceneCtx &sceneCtx);

  bool buildProject(const std::string &path);
}
