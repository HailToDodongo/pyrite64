/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>

#include "vertex.h"
#include "SDL3/SDL_gpu.h"

namespace Renderer
{
  class StorageBuffer
  {
    private:
      SDL_GPUDevice* gpuDevice{nullptr};

      SDL_GPUBuffer* buffer{nullptr};
      SDL_GPUTransferBuffer* bufferTrans{nullptr};

      size_t currByteSize{0};
      bool needsUpload{false};

      void resize(uint32_t newByteSize);

    public:
      StorageBuffer(SDL_GPUDevice* device);
      ~StorageBuffer();

      void setData(char* data, uint32_t dataSize);

      void upload(SDL_GPUCopyPass& pass);
      void bind(SDL_GPURenderPass *pass);
  };
}