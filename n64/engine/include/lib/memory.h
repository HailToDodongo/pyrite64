/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

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
}