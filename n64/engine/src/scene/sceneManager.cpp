/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>

#include "scene/sceneManager.h"

namespace {
  constinit P64::Scene* currScene{nullptr};
  constinit uint32_t sceneId{0};
  constinit uint32_t nextSceneId{0};
}

void P64::SceneManager::load(uint16_t newSceneId) {
  nextSceneId = newSceneId;
}

P64::Scene& P64::SceneManager::getCurrent() {
  return *currScene;
}

// "Private" methods only used in main.cpp
namespace P64::SceneManager
{
  void run()
  {
    sceneId = nextSceneId;
    currScene = new P64::Scene(sceneId);
    while(sceneId == nextSceneId) {
      currScene->update(1.0f / 60.0f);
    }
  }

  void unload()
  {
    delete currScene;
    currScene = nullptr;
  }
}