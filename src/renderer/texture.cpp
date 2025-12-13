/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "texture.h"

#include <cassert>

#include "SDL3_image/SDL_image.h"

extern SDL_GPUSampler *texSamplerRepeat;

Renderer::Texture::Texture(SDL_GPUDevice* device, const std::string &imgPath, int rasterWidth, int rasterHeight)
  : gpuDevice(device)
{
  if(!gpuDevice)return; // CLI mode

  SDL_Surface *imgRaw;
  if (imgPath.ends_with(".svg") && rasterWidth > 0 && rasterHeight > 0) {
    auto imgStream = SDL_IOFromFile(imgPath.c_str(), "rb");
    imgRaw = IMG_LoadSizedSVG_IO(imgStream, rasterWidth, rasterHeight);
  } else {
    imgRaw = IMG_Load(imgPath.c_str());
  }

  auto img = SDL_ConvertSurface(imgRaw, SDL_PIXELFORMAT_BGRA32);
  SDL_DestroySurface(imgRaw);

  width = img->w;
  height = img->h;
  char* image_data = (char*)img->pixels;

  //printf("Loaded %s: w/h: %dx%d, format: %s\n", imgPath.c_str(), width, height, SDL_GetPixelFormatName(img->format));

  // Create texture
  SDL_GPUTextureCreateInfo texture_info = {};
  texture_info.type = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format = SDL_GetGPUTextureFormatFromPixelFormat(img->format);
  texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width = width;
  texture_info.height = height;
  texture_info.layer_count_or_depth = 1;
  texture_info.num_levels = 1;
  texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

  texture = SDL_CreateGPUTexture(device, &texture_info);

  // Create transfer buffer
  // FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
  SDL_GPUTransferBufferCreateInfo transferbuffer_info = {};
  transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transferbuffer_info.size = width * height * 4;
  SDL_GPUTransferBuffer* transferbuffer = SDL_CreateGPUTransferBuffer(device, &transferbuffer_info);
  assert(transferbuffer != nullptr);

  // Copy to transfer buffer
  uint32_t upload_pitch = width * 4;
  void* texture_ptr = SDL_MapGPUTransferBuffer(device, transferbuffer, true);
  for (int y = 0; y < height; y++)
      memcpy((void*)((uintptr_t)texture_ptr + y * upload_pitch), image_data + y * upload_pitch, upload_pitch);
  SDL_UnmapGPUTransferBuffer(device, transferbuffer);

  SDL_GPUTextureTransferInfo transfer_info = {};
  transfer_info.offset = 0;
  transfer_info.transfer_buffer = transferbuffer;

  SDL_GPUTextureRegion texture_region = {};
  texture_region.texture = texture;
  texture_region.x = (Uint32)0;
  texture_region.y = (Uint32)0;
  texture_region.w = (Uint32)width;
  texture_region.h = (Uint32)height;
  texture_region.d = 1;

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
  SDL_UploadToGPUTexture(copy_pass, &transfer_info, &texture_region, false);
  SDL_EndGPUCopyPass(copy_pass);
  SDL_SubmitGPUCommandBuffer(cmd);

  SDL_ReleaseGPUTransferBuffer(device, transferbuffer);

  SDL_DestroySurface(img);

  texBinding.texture = texture;
  texBinding.sampler = texSamplerRepeat;
}

Renderer::Texture::~Texture() {
 SDL_ReleaseGPUTexture(gpuDevice, texture);
}

void Renderer::Texture::bind(SDL_GPURenderPass* pass)
{
  SDL_BindGPUFragmentSamplers(pass, 0, &texBinding, 1);
}
