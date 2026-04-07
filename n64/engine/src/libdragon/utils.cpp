/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "utils.h"
#include "lib/mips.h"
#include "scene/components/animModel.h"
#include "scene/components/model.h"
#include "vi/swapChain.h"

extern "C" {
  // "libdragon/system_internal.h"
  void* sbrk_top(int incr);
}

namespace
{
  int new_display_get_width() {
    return P64::VI::SwapChain::getFrameBuffer(0)->width;
  }

  int new_display_get_height() {
    return P64::VI::SwapChain::getFrameBuffer(0)->height;
  }

  [[noreturn]]
  void notAllowedFn(const char* const fnName)
  {
    assertf(false, "Function '%s' cannot be used!", fnName);
    // if asserts are disabled:
    debugf("Function '%s' cannot be used!\n", fnName);
    for(;;){}
  }

  void disableFunction(void *fn, const char* const fnName)
  {
    uint32_t* instrPtr = (uint32_t*)UncachedAddr(fn);

    instrPtr[0] = MIPS::LUI(MIPS::REG::A0, (uint32_t)fnName >> 16);
    instrPtr[1] = MIPS::JUMP((void*)notAllowedFn);
    instrPtr[2] = MIPS::ORI(MIPS::REG::A0, MIPS::REG::A0, (uint32_t)fnName & 0xFFFF);
  }

  void replaceFunction(auto *fnOld, auto *fnNew)
  {
    uint32_t* instrPtr = (uint32_t*)UncachedAddr(fnOld);

    instrPtr[0] = MIPS::JUMP((void*)fnNew);
    instrPtr[1] = MIPS::NOP();
  }
}

#define DISABLE_FN(fn) disableFunction((void*)fn, #fn)

void P64::LD::init()
{
  // some functions cannot be used since other systems are in place.
  // For example the entire engine uses VI, but libdragon still has display functions.
  // if a user decides to call e.g. display_get_width(), it would return wrong values.
  replaceFunction(display_get_width, new_display_get_width);
  replaceFunction(display_get_height, new_display_get_height);

  DISABLE_FN(display_get_bitdepth);
  DISABLE_FN(display_get_delta_time);
  DISABLE_FN(display_get_fps);
  DISABLE_FN(display_get_num_buffers);
  DISABLE_FN(display_get_current_framebuffer);
  DISABLE_FN(display_get_zbuf);

  DISABLE_FN(t3d_model_get_material);
  DISABLE_FN(t3d_model_draw_material);
  DISABLE_FN(t3d_model_draw);
  DISABLE_FN(t3d_model_draw_custom);
  DISABLE_FN(t3d_model_draw_skinned);
}

void* P64::LD::sbrkSetTop(void* newTop)
{
  void* currentTop = sbrk_top(0);
  int32_t diff = (char*)currentTop - (char*)newTop;
  void* result = sbrk_top(diff);
  assertf(newTop == result, "sbrkSetTop failed: %p != %p", newTop, result);
  return currentTop;
}
