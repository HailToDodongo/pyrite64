/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "selectionUtils.h"

#include <algorithm>

#include "../context.h"
#include "../project/scene/scene.h"
#include "../project/scene/prefab.h"

namespace
{
  std::vector<std::shared_ptr<Project::Object>> collectSelectedObjectRefs(Project::Scene &scene)
  {
    const auto &selected = ctx.getSelectedObjectUUIDs();
    std::vector<std::shared_ptr<Project::Object>> selectedObjects{};
    selectedObjects.reserve(selected.size());
    for (auto uuid : selected) {
      auto obj = scene.getObjectByUUID(uuid);
      if (obj) {
        selectedObjects.push_back(obj);
      }
    }
    return selectedObjects;
  }

  // Follows a sub-path of child ids from a prefab root down to the selected node, hopping
  // through each prefab definition along the way. Returns the chain of visited nodes, or
  // an empty vector when the path no longer resolves.
  std::vector<Project::Object*> walkToNode(Project::Object* root, const std::vector<uint32_t> &subPath)
  {
    std::vector<Project::Object*> nodes;
    Project::Object* cur = root;
    for(uint32_t uid : subPath) {
      Project::Object* parent = Editor::SelectionUtils::prefabDefOf(cur);
      Project::Object* next = nullptr;
      for(auto &c : parent->children) { if(c->uuid == uid) { next = c.get(); break; } }
      if(!next) return {};
      nodes.push_back(next);
      cur = next;
    }
    return nodes;
  }
}

namespace Editor::SelectionUtils
{
  Project::Object* prefabDefOf(Project::Object* node)
  {
    if(node->isPrefabInstance()) {
      auto pf = ctx.project->getAssets().getPrefabByUUID(node->uuidPrefab.value);
      if(pf) return &pf->obj;
    }
    return node;
  }

  AuthTarget pickAuthNode(Project::Object* rootInstance, const std::vector<Project::Object*> &nodes)
  {
    AuthTarget out;
    out.authNode = rootInstance;
    out.relPath = ctx.selSubPath;
    if(ctx.isPrefabEditing(rootInstance->uuid)) {
      int ai = -1;
      for(size_t i = 0; i + 1 < nodes.size(); i++) { // exclude the target itself
        if(nodes[i]->isPrefabInstance()) { ai = (int)i; break; }
      }
      if(ai >= 0) {
        out.authNode = nodes[ai];
        out.relPath.assign(ctx.selSubPath.begin() + ai + 1, ctx.selSubPath.end());
      } else {
        out.directDefEdit = true;
      }
    }
    return out;
  }

  std::vector<Project::Object*> collectSelectedObjects(Project::Scene &scene)
  {
    const auto &selected = ctx.getSelectedObjectUUIDs();
    std::vector<Project::Object*> selectedObjects{};
    selectedObjects.reserve(selected.size());
    for (auto uuid : selected) {
      auto obj = scene.getObjectByUUID(uuid);
      if (obj) {
        selectedObjects.push_back(obj.get());
      }
    }
    return selectedObjects;
  }

  bool deleteSelectedObjects(Project::Scene &scene)
  {
    auto selectedRefs = collectSelectedObjectRefs(scene);
    if (selectedRefs.empty()) {
      return false;
    }

    std::vector<std::shared_ptr<Project::Object>> selectedObjs{};
    selectedObjs.reserve(selectedRefs.size());
    for (auto &selObj : selectedRefs) {
      if (!selObj || !selObj->parent) continue;
      selectedObjs.push_back(selObj);
    }

    if (selectedObjs.empty()) {
      return false;
    }

    auto depthOf = [](Project::Object *obj) {
      int depth = 0;
      while (obj && obj->parent) {
        ++depth;
        obj = obj->parent;
      }
      return depth;
    };

    std::sort(selectedObjs.begin(), selectedObjs.end(), [&](
      const std::shared_ptr<Project::Object> &a,
      const std::shared_ptr<Project::Object> &b
    ) {
      return depthOf(a.get()) > depthOf(b.get());
    });

    for (auto &selObj : selectedObjs) {
      if (!selObj || !selObj->parent) continue;
      scene.removeObject(*selObj);
    }
    ctx.clearObjectSelection();
    return true;
  }

  Project::Object* getPrefabEditObject(Project::Scene &scene)
  {
    if (!ctx.prefabEditUUID) return nullptr;
    auto obj = scene.getObjectByUUID(ctx.prefabEditUUID);
    return obj ? obj.get() : nullptr;
  }

  bool isSelectionAllowed(Project::Scene &scene, uint32_t rootUuid,
                          const std::vector<uint32_t> &subPath)
  {
    auto *editObj = getPrefabEditObject(scene);
    if (!editObj) return true;                       // not editing, anything goes
    if (subPath.empty()) return rootUuid == editObj->uuid;
    if (rootUuid != editObj->uuid) return false;     // a different object's tree

    auto prefab = ctx.project->getAssets().getPrefabByUUID(editObj->uuidPrefab.value);
    if (!prefab) return false;

    auto nodes = walkToNode(&prefab->obj, subPath);
    if (nodes.empty()) return false;                 // stale path

    // A prefab-instance node before the target means we'd be selecting inside a nested
    // prefab, which is not part of this prefab's definition.
    for (size_t i = 0; i + 1 < nodes.size(); i++) {
      if (nodes[i]->isPrefabInstance()) return false;
    }
    return true;
  }

  void resolveNestedTarget(NestedTarget &out, Project::Object* obj, Project::Prefab* prefab)
  {
    if(!prefab || ctx.selSubPath.empty()) return;

    auto nodes = walkToNode(&prefab->obj, ctx.selSubPath);
    if(nodes.empty()) { ctx.selSubPath.clear(); return; } // stale path, fall back to the root

    out.node = nodes.back();
    out.nodeSrc = prefabDefOf(out.node);
    out.isNested = true;

    auto auth = pickAuthNode(obj, nodes);
    out.directDefEdit = auth.directDefEdit;

    if(!out.directDefEdit) {
      out.authLayer.emplace(auth.authNode->propOverrides);
      out.nestedLock.emplace(true); // edits become overrides on authNode

      // Descend relPath, pushing one Path per step and a layer for each nested prefab-instance
      // node along the way (the selected node's own layer is the last). This resolves the same
      // cascade the build does, so an override baked on an intermediate prefab still shows.
      // authNode stays the outermost layer, so it remains the write target.
      size_t startIdx = nodes.size() - auth.relPath.size();
      for(size_t k = 0; k < auth.relPath.size(); k++) {
        out.nestedPaths.push_back(std::make_unique<PropScope::Path>(auth.relPath[k]));
        Project::Object* n = nodes[startIdx + k];
        if(n->isPrefabInstance())
          out.nodeLayers.push_back(std::make_unique<PropScope::PrefabLayer>(n->propOverrides));
      }
    }
  }
}
