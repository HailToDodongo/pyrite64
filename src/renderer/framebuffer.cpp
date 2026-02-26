/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "framebuffer.h"
#include "../context.h"
#include <stdexcept>

SDL_GPUTextureFormat Renderer::getDepthStencilFormat()
{
  constexpr SDL_GPUTextureFormat candidates[] = {
    SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
  };

  for (auto format : candidates) {
    if (SDL_GPUTextureSupportsFormat(ctx.gpu, format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
      return format;
    }
  }

  throw std::runtime_error("No supported depth-stencil texture format found!");
}

Renderer::Framebuffer::Framebuffer()
{
  texInfo.width = 0;
  texInfo.height = 0;
  texInfo.layer_count_or_depth = 1;
  texInfo.num_levels = 1;
  texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

  targetInfo[0].texture = nullptr;
  targetInfo[0].clear_color = {0, 0, 0, 1};
  targetInfo[0].load_op = SDL_GPU_LOADOP_CLEAR;
  targetInfo[0].store_op = SDL_GPU_STOREOP_STORE;
  targetInfo[0].mip_level = 0;
  targetInfo[0].layer_or_depth_plane = 0;
  targetInfo[0].cycle = false;

  targetInfo[1].texture = nullptr;
  targetInfo[1].clear_color = {0, 0, 0, 0};
  targetInfo[1].load_op = SDL_GPU_LOADOP_CLEAR;
  targetInfo[1].store_op = SDL_GPU_STOREOP_STORE;
  targetInfo[1].mip_level = 0;
  targetInfo[1].layer_or_depth_plane = 0;
  targetInfo[1].cycle = false;

  depthTargetInfo.texture = nullptr;
  depthTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  depthTargetInfo.clear_depth = 1;
  depthTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
  depthTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
  depthTargetInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;
  depthTargetInfo.mip_level = 0;
}

Renderer::Framebuffer::~Framebuffer() {
  if (transBufferRead) {
    SDL_ReleaseGPUTransferBuffer(ctx.gpu, transBufferRead);
  }
  if(gpuTex)SDL_ReleaseGPUTexture(ctx.gpu, gpuTex);
  if(gpuTexObj)SDL_ReleaseGPUTexture(ctx.gpu, gpuTexObj);
  if(gpuTexDepth)SDL_ReleaseGPUTexture(ctx.gpu, gpuTexDepth);
}

void Renderer::Framebuffer::resize(uint32_t width, uint32_t height)
{
  if (texInfo.width == width && texInfo.height == height) return;
  texInfo.width = width;
  texInfo.height = height;

  if(gpuTex)SDL_ReleaseGPUTexture(ctx.gpu, gpuTex);
  if(gpuTexObj)SDL_ReleaseGPUTexture(ctx.gpu, gpuTexObj);
  if(gpuTexDepth)SDL_ReleaseGPUTexture(ctx.gpu, gpuTexDepth);

  texInfo.type = SDL_GPU_TEXTURETYPE_2D;
  texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  gpuTex = SDL_CreateGPUTexture(ctx.gpu, &texInfo);

  texInfo.type = SDL_GPU_TEXTURETYPE_2D;
  texInfo.format = SDL_GPU_TEXTUREFORMAT_R32_UINT;
  texInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  gpuTexObj = SDL_CreateGPUTexture(ctx.gpu, &texInfo);

  texInfo.type = SDL_GPU_TEXTURETYPE_2D;
  texInfo.format = getDepthStencilFormat();
  texInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  gpuTexDepth = SDL_CreateGPUTexture(ctx.gpu, &texInfo);

  targetInfo[0].texture = gpuTex;
  targetInfo[1].texture = gpuTexObj;
  depthTargetInfo.texture = gpuTexDepth;
}

void* Renderer::Framebuffer::startGenericRead(uint32_t x, uint32_t y) {
  SDL_GPUCommandBuffer* cmdBuff = SDL_AcquireGPUCommandBuffer(ctx.gpu);
  uint32_t w = 1;
  uint32_t h = 1;

  if (!transBufferRead) {
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = (Uint32)32;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transBufferRead = SDL_CreateGPUTransferBuffer(ctx.gpu, &tbci);
  }

  SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmdBuff);

  SDL_GPUTextureRegion src{};
  src.texture = gpuTexObj;
  src.x = x;
  src.y = y;
  src.w = w;
  src.h = h;
  src.d = 1;

  SDL_GPUTextureTransferInfo dst{};
  dst.transfer_buffer = transBufferRead;
  dst.rows_per_layer = w;
  dst.pixels_per_row = h;

  SDL_DownloadFromGPUTexture(pass, &src, &dst);

  SDL_EndGPUCopyPass(pass);

  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuff);
  SDL_WaitForGPUFences(ctx.gpu, true, &fence, 1);
  SDL_ReleaseGPUFence(ctx.gpu, fence);
  //auto buff = SDL_AcquireGPUCommandBuffer(ctx.gpu);

  return SDL_MapGPUTransferBuffer(ctx.gpu, transBufferRead, false);
}

void Renderer::Framebuffer::endGenericRead() {
  SDL_UnmapGPUTransferBuffer(ctx.gpu, transBufferRead);
}

glm::u8vec4 Renderer::Framebuffer::readColor(uint32_t x, uint32_t y)
{
  if (x >= texInfo.width || y >= texInfo.height) {
    return {0,0,0,0};
  }

  auto data = (uint8_t*)startGenericRead(x,y);
  glm::u8vec4 res{data[0], data[1], data[2], data[3]};
  //printf("Pixel: %02X %02X %02X %02X\n", res[0], res[1], res[2], res[3]);
  endGenericRead();
  return res;
}

uint32_t Renderer::Framebuffer::readObjectID(uint32_t x, uint32_t y) {
  if (x >= texInfo.width || y >= texInfo.height) {
    return 1;
  }

  auto res = *static_cast<uint32_t*>(startGenericRead(x, y));
  //printf("ID: %08X\n", res);
  endGenericRead();
  return res;
}

