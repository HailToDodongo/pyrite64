/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <t3d/t3dmodel.h>
#include <vector>

namespace P64::AssetManager
{
  void reset();
  T3DModel *getT3DM(uint32_t hash, const char* path);
  wav64_t *getWAV(uint32_t hash, const char* path);
}