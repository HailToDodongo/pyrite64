/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <future>

#include "project/project.h"
#include "SDL3/SDL.h"

namespace Renderer { class Scene; }

struct Context
{
  // Globals
  Project::Project *project{nullptr};
  Renderer::Scene *scene{nullptr};
  SDL_Window* window{nullptr};
  SDL_GPUDevice *gpu{nullptr};

  struct Clipboard
  {
    std::string data{};
    uint64_t refUUID{0};
  };

  Clipboard clipboard{};

  // Editor state
  uint64_t selAssetUUID{0};
  uint32_t selObjectUUID{0};

  std::future<void> futureBuildRun{};

  [[nodiscard]] bool isBuildOrRunning() const
  {
    if (futureBuildRun.valid()) {
      auto state = futureBuildRun.wait_for(std::chrono::seconds(0));
      return state != std::future_status::ready;
    }
    return false;
  }
};

extern Context ctx;