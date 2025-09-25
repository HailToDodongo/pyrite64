/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "lib/assetManager.h"
#include "lib/logger.h"

namespace {
  struct AssetEntry {
    uint32_t hash{};
    void* data{};
    void* freeFunc{};
  };

  std::vector<AssetEntry> assets{};

  void* findAsset(uint32_t hash)
  {
    for(auto &entry : assets) {
      if(entry.hash == hash) {
        return (T3DModel*)entry.data;
      }
    }
    return nullptr;
  }
}

void P64::AssetManager::reset() {
  for(auto &entry : assets) {
    ((void (*)(void*))entry.freeFunc)(entry.data);
  }
  assets.clear();
  assets.shrink_to_fit();
}

T3DModel *P64::AssetManager::getT3DM(uint32_t hash, const char *path) {
  if(auto *asset = (T3DModel*)findAsset(hash)) {
    return asset;
  }

  auto *model = t3d_model_load(path);
  if(!model) {
    Log::error("Failed to load model");
    return nullptr;
  }

  assets.push_back({hash, model, (void*)t3d_model_free});
  return model;

}

wav64_t *P64::AssetManager::getWAV(uint32_t hash, const char *path) {
  if(auto *asset = (wav64_t*)findAsset(hash)) {
    return asset;
  }
  auto *wav = wav64_load(path, nullptr);
  if(!wav) {
    Log::error("Failed to load wav");
    return nullptr;
  }
  assets.push_back({hash, wav, (void*)wav64_close});
  return wav;
}
