#include "script/globalScript.h"
#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace P64::GlobalScript::__UUID__
{
  // The following functions are called by the engine at different points in the games lifecycle.
  // If you don't need a specific function you can remove it.
  // This script is hooked up automatically and doesn't need any setup in the editor.

  void onGameInit()
  {
    // called once when the game starts, all other systems (e.g. asset manager) are already inititalized
  }

  void onScenePreLoad()
  {
    // right before a new scene is loaded
  }

  void onScenePostLoad()
  {
    // right after a new scene was loaded, but before any update has happened yet
  }

  void onScenePreUnload()
  {
    // right before a scene is unloaded
  }

  void onScenePostUnload()
  {
    // right after a scene was unloaded
  }

  void onSceneUpdate()
  {
    // called once per frame during the update phase of the frame.
    // this is done before all the object updates.
  }

  void onScenePreDraw()
  {
    // called once per frame during the draw phase of the frame.
    // this is done before all the object draws, *NOT* per camera, just once.
    // The default 3D layer is active and must be restored if changed.
  }

  void onScenePreDraw3D()
  {
    // called once per frame per camera.
    // this is done before all object draws for the specific camera.
    // The default 3D layer is active and must be restored if changed.
  }

  void onScenePostDraw3D()
  {
    // called once per frame per camera.
    // this is done after all objects are drawn for the specific camera.
    // The default 3D layer is active and must be restored if changed.
  }

  void onSceneDraw2D()
  {
    // called once per frame after all cameras/objects are drawn.
    // The default 2D layer is active and must be restored if changed.
  }
}
