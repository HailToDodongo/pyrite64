/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  class Scene;
}

/**
 * Functions to manager scenes.
 */
namespace P64::SceneManager
{
  /**
   * Request loading a scene by ID.
   * Note that the actual load will happen at the end of the current frame.
   * If this function was called multiple times, the last ID will be used.
   *
   * This function does nothing if the provided ID is the current scene.
   * If you want to force-reload the current scene, use reload() instead.
   * @param sceneId scene to load
   */
  void load(uint16_t newSceneId);

  /**
   * Reload the current scene.
   * This reload is deferred to the end of the current frame.
   */
  void reload();

  /**
   * Returns the current scene.
   * @return scene, never NULL
   */
  Scene &getCurrent();
}