/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <filesystem>
#include "sceneContext.h"
#include "../project/project.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Build
{
  typedef bool(*BuildFunc)(Project::Project &project, SceneCtx &sceneCtx);

  // World transform passed down while expanding prefab instances. The engine has no
  // runtime transform hierarchy, so expanded nested objects get their world transform
  // baked here, matching how the runtime renders objects.
  struct WorldTransform
  {
    glm::vec3 pos{0,0,0};
    glm::quat rot{glm::vec3(0.0f)};
    glm::vec3 scale{1,1,1};
  };

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
    bool engineSrc{false};
  };
  bool cleanProject(const Project::Project &project, const CleanArgs &args = {});

  // individual parts
  // runtimeId and parentRuntimeId are this node's ids. 'expanding' is true once the walk
  // has descended into a prefab definition. Nested children then get build-time ids from
  // ctx.nextRuntimeId rather than the scene-assigned ones.
  uint32_t writeObject(SceneCtx &ctx, Project::Object &obj, bool savePrefabItself = false,
                       uint16_t runtimeId = 0, uint16_t parentRuntimeId = 0, bool expanding = false,
                       const WorldTransform &parentTransform = {}, bool isPrefabRoot = false);

  bool buildT3DCollision(
    Project::Project &project, SceneCtx &sceneCtx,
    const std::unordered_set<std::string> &meshes,
    uint64_t orgUUID,
    uint64_t newUUID
  );

  Utils::BinaryFile buildCollision(const std::string &gltfPath, float baseScale, const std::unordered_set<std::string> &meshes = {});
}
