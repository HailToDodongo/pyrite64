/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>

#include "scene/sceneManager.h"

#include "scene/scene.h"
#include "script/globalScript.h"
#include "vi/swapChain.h"

namespace {
  constinit P64::Scene* currScene{nullptr};
  constinit uint32_t sceneId{0};
  constinit uint32_t nextSceneId{0};
  constinit uint8_t forceReload{0};
}

// "Private" methods only used in main.cpp
namespace P64::SceneManager
{
  void load(uint16_t newSceneId) {
    nextSceneId = newSceneId;
  }

  void reload() {
    nextSceneId = sceneId;
    forceReload = 1;
  }

  Scene& getCurrent() {
    return *currScene;
  }

  void run()
  {
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_LOAD);

    sceneId = nextSceneId;
    currScene = new P64::Scene(sceneId, &currScene);

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_LOAD);

    while(sceneId == nextSceneId && !forceReload) {
      currScene->update(VI::SwapChain::getDeltaTime());
    }
    forceReload = 0;
  }

  void unload()
  {
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_UNLOAD);
    delete currScene;
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_UNLOAD);
    currScene = nullptr;
  }
}