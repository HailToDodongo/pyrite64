/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../utils/prop.h"
#include "imgui/helper.h"

namespace Project {
  class Scene;
  class Object;
  class Prefab;
}

namespace Editor::SelectionUtils
{
  std::vector<Project::Object*> collectSelectedObjects(Project::Scene &scene);

  bool deleteSelectedObjects(Project::Scene &scene);

  // The scene object currently in prefab-edit mode, or nullptr. While one is active,
  // selection is restricted to that prefab's own definition. See isSelectionAllowed.
  Project::Object* getPrefabEditObject(Project::Scene &scene);

  // Whether a selection may target rootUuid plus the nested subPath in the current mode.
  // With no edit mode anything is allowed. In edit mode only the edited prefab's own
  // definition is selectable, meaning the instance itself, plain nested children, and
  // nested prefab-instance nodes. The contents of those nested prefabs are not, so a
  // prefab-instance ancestor on the path makes the node unselectable.
  bool isSelectionAllowed(Project::Scene &scene, uint32_t rootUuid,
                          const std::vector<uint32_t> &subPath);

  // Resolves a node to the prefab definition whose children it exposes. A prefab-instance
  // node has no children of its own, so this returns the referenced prefab's root, otherwise
  // the node itself (also when the prefab cannot be found).
  Project::Object* prefabDefOf(Project::Object* node);

  // Where a nested node's edits are written, given the chain of def nodes leading to it.
  struct AuthTarget
  {
    Project::Object* authNode = nullptr;  // object whose override map the edit is stored on
    std::vector<uint32_t> relPath;        // path from authNode down to the edited node
    bool directDefEdit = false;           // edit the def node directly, no override layer
  };

  // Decides where a nested node's edits go. In normal mode that is the scene instance with
  // the full selSubPath. In prefab-edit mode it is the first contained prefab-instance along
  // the path, whose overrides belong to the prefab being edited, or a direct definition edit
  // when no such prefab sits in between.
  AuthTarget pickAuthNode(Project::Object* rootInstance, const std::vector<Project::Object*> &nodes);

  // A nested prefab selection resolved for the inspector: which def node to show, and the
  // live scope guards that route its edits to the correct override owner. The guards are
  // non-movable, so the caller keeps this as a local for as long as it renders the node.
  struct NestedTarget
  {
    Project::Object* node = nullptr;     // inspected nested def node, null when not nested
    Project::Object* nodeSrc = nullptr;  // its component source, its prefab root if instance
    bool isNested = false;
    bool directDefEdit = false;
    std::optional<PropScope::PrefabLayer> authLayer;
    std::vector<std::unique_ptr<PropScope::Path>> nestedPaths;
    std::optional<ImTable::ForceLockScope> nestedLock;
    // One layer per prefab-instance node between the author node and the selected node (the selected node's own layer is the last).
    // Keeps the inspector's cascade matching the build.
    std::vector<std::unique_ptr<PropScope::PrefabLayer>> nodeLayers;
  };

  // Decides where a nested node's edits are written and sets up the matching scope guards.
  // In normal mode that is the scene instance with the full path. In prefab-edit mode it is
  // the first contained prefab-instance along the path, whose overrides belong to the prefab
  // being edited, or a direct definition edit when no such prefab sits in between. Fills
  // 'out' in place to keep the guards alive.
  void resolveNestedTarget(NestedTarget &out, Project::Object* obj, Project::Prefab* prefab);
}
