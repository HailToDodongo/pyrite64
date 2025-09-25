/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "lib/memory.h"

extern "C" {
  void* sbrk_top(int incr);
}

namespace {
  surface_t surfDepth{};
  bool usedAlloc{false};

  heap_stats_t heapStats{};
}

namespace P64::Mem
{
  surface_t& allocDepthBuffer(uint32_t width, uint32_t height)
  {
    if(!surfDepth.buffer || surfDepth.width != width || surfDepth.height != height)
    {
      freeDepthBuffer();
      void *buf = sbrk_top(width * height * 2);
      if((int)buf == -1) {
        surfDepth = surface_alloc(FMT_RGBA16, width, height);
        usedAlloc = true;
      } else {
        data_cache_hit_invalidate(buf, width * height * 2);
        surfDepth = surface_make(UncachedAddr(buf), FMT_RGBA16, width, height, width*2);
        usedAlloc = false;
      }
    }

    return surfDepth;
  }

  void freeDepthBuffer()
  {
    if(surfDepth.buffer) {
      if(usedAlloc) {
        surface_free(&surfDepth);
      } else {
        sbrk_top(-(surfDepth.width * surfDepth.height * 2));
      }
    }
    surfDepth.buffer = nullptr;
  }

  int32_t getHeapDiff()
  {
    auto oldStats = heapStats;
    sys_get_heap_stats(&heapStats);
    if(oldStats.total == 0)return 0; // first call
    return heapStats.used - oldStats.used;
  }
}