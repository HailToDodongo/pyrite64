/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/components/surface.h"

namespace
{
  // Converts the packed 32-bit RGBA clear-color into a 64-bit fill pattern.
  // For intensity formats the red channel is used, for CI formats it serves as the palette index.
  uint64_t makeClearPattern(tex_format_t fmt, uint32_t packed32)
  {
    color_t c = color_from_packed32(packed32);
    uint64_t pat{};
    switch(fmt)
    {
      case FMT_RGBA32: pat = packed32;                              break;
      case FMT_RGBA16: pat = color_to_packed16(c);                  break;
      case FMT_IA16:   pat = ((uint32_t)c.r << 8) | c.a;            break;
      case FMT_IA8:    pat = (c.r & 0xF0) | (c.a >> 4);             break;
      case FMT_IA4:    pat = ((c.r >> 5) << 1) | (c.a >> 7);
                       pat |= pat << 4;                             break;
      case FMT_I8:
      case FMT_CI8:    pat = c.r;                                   break;
      case FMT_I4:
      case FMT_CI4:    pat = c.r >> 4;
                       pat |= pat << 4;                             break;
      default:         return 0;
    }

    if(TEX_FORMAT_BITDEPTH(fmt) <= 8) pat |= pat << 8;
    if(TEX_FORMAT_BITDEPTH(fmt) <= 16) pat |= pat << 16;
    return pat | (pat << 32);
  }
}

namespace P64::Comp
{
  void Surface::initDelete([[maybe_unused]] Object& obj, Surface* data, InitData* initData)
  {
    if(initData == nullptr) {
      for(uint32_t i=0; i<data->buffCount; ++i) {
        surface_free(&data->buffers[i]);
      }
      return;
    }

    auto fmt = (tex_format_t)initData->format;
    data->buffCount = initData->buffCount < 1 ? 1
      : (initData->buffCount > MAX_BUFFERS ? MAX_BUFFERS : initData->buffCount);
    data->currIdx = 0;
    data->flags = initData->flags;
    data->clearPattern = makeClearPattern(fmt, initData->clearColor);

    for(uint32_t i=0; i<data->buffCount; ++i) {
      data->buffers[i] = surface_alloc(fmt, initData->width, initData->height);
      data->clear(data->buffers[i]);
    }
  }

  void Surface::update([[maybe_unused]] Object& obj, Surface* data, [[maybe_unused]] float deltaTime)
  {
    if(data->buffCount > 1) {
      data->currIdx = (data->currIdx + 1) % data->buffCount;
    }
    if(data->flags & FLAG_CLEAR) {
      data->clear();
    }
  }
}
