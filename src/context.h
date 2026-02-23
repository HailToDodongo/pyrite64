/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <algorithm>
#include <future>
#include <vector>

#include "editor/inputConfig.h"
#include "project/project.h"
#include "utils/toolchain.h"
#include "SDL3/SDL.h"

namespace Editor
{
  class Scene;
}

namespace Renderer { class Scene; }

struct Context
{
  // Globals
  Utils::Toolchain toolchain{};
  Project::Project *project{nullptr};
  Renderer::Scene *scene{nullptr};
  SDL_Window* window{nullptr};
  SDL_GPUDevice *gpu{nullptr};
  std::unique_ptr<Editor::Scene> editorScene{nullptr};

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
  Editor::InputConfig inputs{};

  std::future<void> futureBuildRun{};

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
  }

  void setObjectSelection(uint32_t uuid)
  {
    selObjectUUIDs.clear();
    if (uuid != 0) {
      selObjectUUIDs.push_back(uuid);
      selObjectUUID = uuid;
      return;
    }
    selObjectUUID = 0;
  }

  void setObjectSelectionList(const std::vector<uint32_t> &uuids, uint32_t primaryUUID)
  {
    selObjectUUIDs = uuids;
    selObjectUUID = primaryUUID;
    if (!isObjectSelected(selObjectUUID)) {
      selObjectUUID = selObjectUUIDs.empty() ? 0 : selObjectUUIDs.back();
    }
  }

  void addObjectSelection(uint32_t uuid)
  {
    if (uuid == 0) return;
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
  }
};

extern Context ctx;