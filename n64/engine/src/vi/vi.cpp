/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "vi.h"
#include <libdragon.h>

float P64::VI::calcRefreshRate()
{
 auto tv_type = get_tv_type();
 int clock;
 switch (tv_type) {
      case TV_PAL:    clock = 49656530; break;
      case TV_MPAL:   clock = 48628322; break;
      default:        clock = 48681818; break;
  }
  uint32_t HSYNC = *VI_H_TOTAL;
  uint32_t VSYNC = *VI_V_TOTAL;
  uint32_t HSYNC_LEAP = *VI_H_TOTAL_LEAP;

  int h_sync = (HSYNC & 0xFFF) + 1;
  int v_sync = (VSYNC & 0x3FF) + 1;
  int h_sync_leap_b = (HSYNC_LEAP >>  0) & 0xFFF;
  int h_sync_leap_a = (HSYNC_LEAP >> 16) & 0xFFF;
  int leap_bitcount = 0;
  int mask = 1<<16;
  for (int i=0; i<5; i++) {
      if (HSYNC & mask) leap_bitcount++;
      mask <<= 1;
  }
  int h_sync_leap_avg = (h_sync_leap_a * leap_bitcount + h_sync_leap_b * (5 - leap_bitcount)) / 5;

  return (float)clock / ((h_sync * (v_sync - 2) / 2 + h_sync_leap_avg));
}
