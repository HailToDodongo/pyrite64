/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "texture.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "SDL3_image/SDL_image.h"

extern SDL_GPUSampler *texSamplerRepeat;

Renderer::Texture::Texture(SDL_GPUDevice* device, const std::string &imgPath, bool isMono, int rasterWidth, int rasterHeight)
  : gpuDevice(device), path(imgPath), isMono(isMono), rasterWidth(rasterWidth), rasterHeight(rasterHeight)
{
  // Only decode to record the size here, the pixels are read again on the first GPU use.
  auto img = decode();
  if(!img) {
    printf("Failed to load image '%s': %s\n", imgPath.c_str(), SDL_GetError());
    return;
  }

  width = img->w;
  height = img->h;
  SDL_DestroySurface(img);
}

SDL_Surface* Renderer::Texture::decode() const
{
  SDL_Surface *imgRaw;
  if (path.ends_with(".svg") && rasterWidth > 0 && rasterHeight > 0) {
    auto imgStream = SDL_IOFromFile(path.c_str(), "rb");
    imgRaw = IMG_LoadSizedSVG_IO(imgStream, rasterWidth, rasterHeight);
    if(imgStream)SDL_CloseIO(imgStream);
  } else {
    imgRaw = IMG_Load(path.c_str());
  }
  if(!imgRaw)return nullptr;

  auto img = SDL_ConvertSurface(imgRaw, SDL_PIXELFORMAT_BGRA32);
  SDL_DestroySurface(imgRaw);
  if(!img)return nullptr;

  if(isMono)
  {
    SDL_LockSurface(img);
    for (int y = 0; y < img->h; y++) {
      for (int x = 0; x < img->w; x++) {
        uint8_t* pixel = (uint8_t*)img->pixels + y * img->pitch + x * 4;
        // gamma correction

        /*float r = pixel[0] / 255.0f;
        r = std::pow(r, 1.0f/2.2f);
        uint8_t gray = (uint8_t)(SDL_clamp(r * 255.0f, 0.0f, 255.0f));
        pixel[3] = gray;*/
        pixel[3] = pixel[0];
      }
    }
    SDL_UnlockSurface(img);
  }

  return img;
}

void Renderer::Texture::ensureUploaded() const
{
  if(uploadTried)return;
  uploadTried = true; // a failed decode must not retry every frame
  if(!gpuDevice || width <= 0)return; // CLI mode or broken image

  auto img = decode();
  if(!img)return;

  char* image_data = (char*)img->pixels;

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

  texture = SDL_CreateGPUTexture(gpuDevice, &texture_info);

  // Create transfer buffer
  // FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
  SDL_GPUTransferBufferCreateInfo transferbuffer_info = {};
  transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transferbuffer_info.size = width * height * 4;
  SDL_GPUTransferBuffer* transferbuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferbuffer_info);
  assert(transferbuffer != nullptr);

  // Copy to transfer buffer, the GPU side is tightly packed while the surface may be padded
  uint32_t upload_pitch = width * 4;
  void* texture_ptr = SDL_MapGPUTransferBuffer(gpuDevice, transferbuffer, true);
  for (int y = 0; y < height; y++)
      memcpy((void*)((uintptr_t)texture_ptr + y * upload_pitch), image_data + y * img->pitch, upload_pitch);
  SDL_UnmapGPUTransferBuffer(gpuDevice, transferbuffer);

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

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
  SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
  SDL_UploadToGPUTexture(copy_pass, &transfer_info, &texture_region, false);
  SDL_EndGPUCopyPass(copy_pass);
  SDL_SubmitGPUCommandBuffer(cmd);

  SDL_ReleaseGPUTransferBuffer(gpuDevice, transferbuffer);

  SDL_DestroySurface(img);

  texBinding.texture = texture;
  texBinding.sampler = texSamplerRepeat;
}

Renderer::Texture::~Texture() {
  if(gpuDevice && texture)SDL_ReleaseGPUTexture(gpuDevice, texture);
}

void Renderer::Texture::bind(SDL_GPURenderPass* pass)
{
  ensureUploaded();
  SDL_BindGPUFragmentSamplers(pass, 0, &texBinding, 1);
}
