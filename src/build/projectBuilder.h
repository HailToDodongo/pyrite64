/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <filesystem>
#include "sceneContext.h"
#include "../project/project.h"

namespace Build
{
  typedef bool(*BuildFunc)(Project::Project &project, SceneCtx &sceneCtx);

  // helper
  bool assetBuildNeeded(const Project::AssetManagerEntry &asset, const fs::path &outPath);

  // Asset builds
  void buildScene(Project::Project &project, const Project::SceneEntry &scene, SceneCtx &ctx);
  void buildScripts(Project::Project &project, SceneCtx &sceneCtx);
  void buildGlobalScripts(Project::Project &project, SceneCtx &sceneCtx);

  bool buildT3DMAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildFontAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildTextureAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildAudioAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildPrefabAssets(Project::Project &project, SceneCtx &sceneCtx);
  bool buildNodeGraphAssets(Project::Project &project, SceneCtx &sceneCtx);

  bool buildProject(const std::string &path);

  struct CleanArgs
  {
    bool code{true};
    bool assets{true};
    bool engine{true};
  };
  bool cleanProject(const Project::Project &project, const CleanArgs &args = {});

  // individual parts
  uint32_t writeObject(SceneCtx &ctx, Project::Object &obj, bool savePrefabItself = false);

  bool buildT3DCollision(
    Project::Project &project, SceneCtx &sceneCtx,
    const std::unordered_set<std::string> &meshes,
    uint64_t orgUUID,
    uint64_t newUUID
  );

  Utils::BinaryFile buildCollision(const std::string &gltfPath, float baseScale, const std::unordered_set<std::string> &meshes = {});
}
