/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

extern "C" {
  extern uint32_t __text_start[];
  extern uint32_t __text_end[];
  extern uint32_t __data_start[];
  extern uint32_t __data_end[];
  extern uint32_t __bss_start[];
}

namespace P64::Mem
{
  /**
   * Lazily allocates a depth buffer of the given size.
   * If already allocated, it will return the same buffer.
   * If the given size is different, the old buffer will be freed and a new one created.
   *
   * @param width
   * @param height
   * @return
   */
  surface_t& allocDepthBuffer(uint32_t width, uint32_t height);

  /**
   * Free the depth buffer, NOP if none was allocated.
   * This is also done automatically in 'allocDepthBuffer' if re-allocation is needed.
   */
  void freeDepthBuffer();

  /**
   * Checks heap stats to detect memory leaks.
   * This will intern remember the heap from the last time it was called
   * and compare it against the current heap.
   *
   * @return heap-different in bytes
   */
  int32_t getHeapDiff();

  inline void clearSurface(surface_t &surf) {
    sys_hw_memset64(surf.buffer, 0, surf.height * surf.stride);
  }

  struct StaticMem
  {
    uint32_t total{};
    uint32_t text{};
    uint32_t data{};
    uint32_t bss{};
    uint32_t misc{};
  };

  inline StaticMem getStaticMemInfo()
  {
    StaticMem res{
      .total = ((uint32_t)__bss_end) & 0x00FF'FFFF,
      .text = ((uint32_t)__text_end) - ((uint32_t)__text_start),
      .data = ((uint32_t)__data_end) - ((uint32_t)__data_start),
      .bss = ((uint32_t)__bss_end) - ((uint32_t)__bss_start),
    };
    res.misc = res.total - res.text - res.data - res.bss;
    return res;
  }
}