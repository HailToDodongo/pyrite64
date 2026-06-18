/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "storageBuffer.h"

Renderer::StorageBuffer::StorageBuffer(SDL_GPUDevice* device)
  : gpuDevice{device}
{
}

Renderer::StorageBuffer::~StorageBuffer()
{
  SDL_ReleaseGPUBuffer(gpuDevice, buffer);
  SDL_ReleaseGPUTransferBuffer(gpuDevice, bufferTrans);
}

void Renderer::StorageBuffer::resize(uint32_t newByteSize) {
  assert(newByteSize != 0);

  SDL_GPUBufferCreateInfo bufferInfo{
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = newByteSize
  };
  buffer = SDL_CreateGPUBuffer(gpuDevice, &bufferInfo);
  assert(buffer != nullptr);

  SDL_GPUTransferBufferCreateInfo transferInfo{
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = newByteSize
  };
  bufferTrans = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
  assert(bufferTrans != nullptr);
}

void Renderer::StorageBuffer::setData(char* data, uint32_t dataSize)
{
  if (dataSize != currByteSize) {
    resize(dataSize);
  }
  currByteSize = dataSize;

// @TODO: store vert/indices in one single buffer

  auto buff = SDL_MapGPUTransferBuffer(gpuDevice, bufferTrans, true);
  SDL_memcpy(buff, data, dataSize);
  SDL_UnmapGPUTransferBuffer(gpuDevice, bufferTrans);

  needsUpload = true;
}

void Renderer::StorageBuffer::upload(SDL_GPUCopyPass &pass) {
  if (!needsUpload || currByteSize == 0)return;

  SDL_GPUTransferBufferLocation location{};
  location.transfer_buffer = bufferTrans;
  location.offset = 0;

  SDL_GPUBufferRegion region{};
  region.buffer = buffer;
  region.size = currByteSize;
  region.offset = 0;
  SDL_UploadToGPUBuffer(&pass, &location, &region, true);
  needsUpload = false;
}

void Renderer::StorageBuffer::bind(SDL_GPURenderPass* pass)
{
  if(buffer) {
    SDL_GPUBuffer* buffers[] = {buffer};
    SDL_BindGPUVertexStorageBuffers(pass, 0, buffers, 1);
  }
}
