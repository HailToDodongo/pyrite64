/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <functional>

namespace P64::VI::SwapChain
{
  using RenderPassCB = void(*)(uint32_t fbIndex);
  using RenderPassDrawTask = std::function<void(surface_t* fb, uint32_t fbIndex, RenderPassCB done)>;

  void init();

  float getDeltaTime();
  float getFPS();

  void nextFrame();
  void drain();
  void setFrameSkip(uint32_t skip);

  void setDrawPass(RenderPassDrawTask task);
  void start();

  surface_t *getFrameBuffer(uint32_t idx);
  void setFrameBuffers(surface_t buffers[3]);
}