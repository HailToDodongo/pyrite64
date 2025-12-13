/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

namespace P64::GlobalScript
{
  typedef void(*Func)();

  enum class HookType
  {
    GAME_INIT = 0,

    // Loading / Unloading
    SCENE_PRE_LOAD,
    SCENE_POST_LOAD,
    SCENE_PRE_UNLOAD,
    SCENE_POST_UNLOAD,

    // Logic
    SCENE_UPDATE,

    // Drawing
    SCENE_PRE_DRAW_3D,
    SCENE_POST_DRAW_3D,
    SCENE_PRE_DRAW_2D,
    SCENE_POST_DRAW_2D,

    _size_,
  };

  void callHooks(HookType type);
}