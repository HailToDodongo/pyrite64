/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <vector>

#include "project/project.h"
#include "utils/json.h"
#include "utils/jsonBuilder.h"
#include "utils/proc.h"
#include "utils/toolchain.h"
#include "SDL3/SDL.h"
#include "editor/keymap.h"
#include "editor/preferences.h"

namespace Editor
{
  class Scene;
  class ThumbnailCache;
}

namespace Renderer { class Scene; }

struct Context
{
  // Globals
  bool debugMode{false};
  Utils::Toolchain toolchain{};
  Project::Project *project{nullptr};
  Renderer::Scene *scene{nullptr};
  Editor::ThumbnailCache *thumbnails{nullptr};
  SDL_Window* window{nullptr};
  SDL_GPUDevice *gpu{nullptr};
  std::unique_ptr<Editor::Scene> editorScene{nullptr};
  bool forceVSync{false};
  bool experimentalFeatures{false};
  bool wantsProjectClose{false};

  std::string newerVersion{};
  std::atomic_bool hasNewerVersion{false};

  struct Clipboard
  {
    struct Entry {
      std::string data{};
      uint64_t refUUID{0};
    };

    std::vector<Entry> entries{};
  };

  Clipboard clipboard{};

  uint64_t timeCpuSelf{};
  uint64_t timeCpuTotal{};

  // Editor state
  uint64_t selAssetUUID{0};
  uint32_t selObjectUUID{0}; // The "primary" selected object (for single selection or the most recently selected in multi-selection)
  std::vector<uint32_t> selObjectUUIDs{}; // All selected object UUIDs (for multi-selection, includes selObjectUUID as the last element)
  // When non-empty, the selection targets a nested prefab-definition object below
  // selObjectUUID (a prefab instance). The path is the chain of definition-node uuids
  // from the instance's prefab root down to the nested node. Edits become overrides on
  // the instance, keyed by this path.
  std::vector<uint32_t> selSubPath{};

  // UUID of the object whose prefab is being edited in place, 0 when not editing.
  uint32_t prefabEditUUID{0};

  Editor::Preferences prefs{};

  std::future<void> futureBuildRun{};

  // Actions deferred until after the current frame's GPU render
  std::vector<std::function<void()>> deferredActions{};
  void deferAction(std::function<void()> fn) { deferredActions.push_back(std::move(fn)); }
  void runDeferredActions()
  {
    auto actions = std::move(deferredActions);
    deferredActions.clear();
    for (auto &fn : actions) fn();
  }

  [[nodiscard]] bool isBuildOrRunning() const
  {
    if (futureBuildRun.valid()) {
      auto state = futureBuildRun.wait_for(std::chrono::seconds(0));
      return state != std::future_status::ready;
    }
    return false;
  }

  void clearObjectSelection()
  {
    selObjectUUID = 0;
    selObjectUUIDs.clear();
    selSubPath.clear();
  }

  void setObjectSelection(uint32_t uuid)
  {
    selObjectUUIDs.clear();
    selSubPath.clear();
    if (uuid != 0) {
      selObjectUUIDs.push_back(uuid);
      selObjectUUID = uuid;
      return;
    }
    selObjectUUID = 0;
  }

  // Selects a nested prefab-definition object. `rootUuid` is the instance and `path` is
  // the chain of definition-node uuids down to the nested node.
  void setNestedSelection(uint32_t rootUuid, const std::vector<uint32_t> &path)
  {
    selObjectUUIDs.clear();
    selObjectUUID = rootUuid;
    if (rootUuid != 0) selObjectUUIDs.push_back(rootUuid);
    selSubPath = path;
  }

  void setObjectSelectionList(const std::vector<uint32_t> &uuids, uint32_t primaryUUID)
  {
    selSubPath.clear(); // a flat multi-selection is never a nested-prefab selection
    selObjectUUIDs = uuids;
    selObjectUUID = primaryUUID;
    if (!isObjectSelected(selObjectUUID)) {
      selObjectUUID = selObjectUUIDs.empty() ? 0 : selObjectUUIDs.back();
    }
  }

  void addObjectSelection(uint32_t uuid)
  {
    if (uuid == 0) return;
    selSubPath.clear();
    if (!isObjectSelected(uuid)) {
      selObjectUUIDs.push_back(uuid);
    }
    selObjectUUID = uuid;
  }

  void removeObjectSelection(uint32_t uuid)
  {
    if (uuid == 0) return;
    auto it = std::remove(selObjectUUIDs.begin(), selObjectUUIDs.end(), uuid);
    if (it != selObjectUUIDs.end()) {
      selObjectUUIDs.erase(it, selObjectUUIDs.end());
    }

    if (selObjectUUID == uuid) {
      selObjectUUID = selObjectUUIDs.empty() ? 0 : selObjectUUIDs.back();
    }
  }

  void toggleObjectSelection(uint32_t uuid)
  {
    if (isObjectSelected(uuid)) {
      removeObjectSelection(uuid);
    } else {
      addObjectSelection(uuid);
    }
  }

  [[nodiscard]] bool isObjectSelected(uint32_t uuid) const
  {
    if (uuid == 0) return false;
    return std::find(selObjectUUIDs.begin(), selObjectUUIDs.end(), uuid) != selObjectUUIDs.end();
  }

  [[nodiscard]] bool isPrefabEditing(uint32_t uuid) const
  {
    return uuid != 0 && uuid == prefabEditUUID;
  }

  [[nodiscard]] const std::vector<uint32_t>& getSelectedObjectUUIDs() const
  {
    return selObjectUUIDs;
  }

  // Ensure that the selected object UUIDs are valid in the current scene, and update selObjectUUID accordingly
  void sanitizeObjectSelection(Project::Scene* scene)
  {
    if (!scene) {
      clearObjectSelection();
      return;
    }

    auto keepIt = std::remove_if(
      selObjectUUIDs.begin(),
      selObjectUUIDs.end(),
      [scene](uint32_t uuid) {
        return !scene->getObjectByUUID(uuid);
      }
    );
    if (keepIt != selObjectUUIDs.end()) {
      selObjectUUIDs.erase(keepIt, selObjectUUIDs.end());
    }

    if (!isObjectSelected(selObjectUUID)) {
      selObjectUUID = selObjectUUIDs.empty() ? 0 : selObjectUUIDs.back();
    }

    // Drop prefab-edit mode if its object is gone (deleted, or scene switched).
    if (prefabEditUUID && !scene->getObjectByUUID(prefabEditUUID)) prefabEditUUID = 0;
  }
};

extern Context ctx;