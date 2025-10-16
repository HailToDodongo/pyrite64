/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "framebuffer.h"
#include "../context.h"

Renderer::Framebuffer::Framebuffer()
{
  texInfo.width = 0;
  texInfo.height = 0;
  texInfo.layer_count_or_depth = 1;
  texInfo.num_levels = 1;
  texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

  targetInfo.texture = nullptr;
  targetInfo.clear_color = {0, 0, 0, 1};
  targetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  targetInfo.store_op = SDL_GPU_STOREOP_STORE;
  targetInfo.mip_level = 0;
  targetInfo.layer_or_depth_plane = 0;
  targetInfo.cycle = false;

  depthTargetInfo.texture = nullptr;
  depthTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  depthTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
  depthTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
  depthTargetInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;
  depthTargetInfo.mip_level = 0;
}

Renderer::Framebuffer::~Framebuffer() {
  if(gpuTex) {
    SDL_ReleaseGPUTexture(ctx.gpu, gpuTex);
  }
}

void Renderer::Framebuffer::resize(uint32_t width, uint32_t height)
{
  if (texInfo.width == width && texInfo.height == height) return;
  texInfo.width = width;
  texInfo.height = height;

  if(gpuTex) {
    SDL_ReleaseGPUTexture(ctx.gpu, gpuTex);
  }

  texInfo.type = SDL_GPU_TEXTURETYPE_2D;
  texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  gpuTex = SDL_CreateGPUTexture(ctx.gpu, &texInfo);

  texInfo.type = SDL_GPU_TEXTURETYPE_2D;
  texInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
  texInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  gpuTexDepth = SDL_CreateGPUTexture(ctx.gpu, &texInfo);

  targetInfo.texture = gpuTex;
  depthTargetInfo.texture = gpuTexDepth;
}
