/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "selectionUtils.h"

#include <algorithm>

#include "../context.h"
#include "../project/scene/scene.h"

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
}

namespace Editor::SelectionUtils
{
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
}
