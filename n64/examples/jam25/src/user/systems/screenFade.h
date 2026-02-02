/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

namespace P64::User::ScreenFade
{
  void fadeOut(int type, float time);
  void fadeIn(int type, float time);

  void setFadeState(bool isFadedIn);

  bool isDone();

  void draw();
}