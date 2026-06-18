/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include <SDL3/SDL_gpu.h>
#include "imgui.h"

#include "../renderer/framebuffer.h"
#include "../renderer/object.h"
#include "../renderer/skeleton.h"
#include "../renderer/uniforms.h"
#include "../project/component/shared/materialInstance.h"
#include "../project/scene/object.h"

namespace Renderer { class Scene; }

namespace Editor
{
  // Renders cached, multi-angle preview images for 3D models.
  // Generation is deferred (one angle per frame) so populating many thumbnails
  // never stalls the main thread. Results are cached in memory (an atlas texture)
  // and on disk as a PNG, keyed by asset UUID and invalidated by source mtime.
  class ThumbnailCache
  {
    public:
      static constexpr int ANGLE_COUNT   = 8;
      static constexpr int TILE_SIZE     = 128;
      static constexpr int TILES_PER_ROW = 4;
      static constexpr int ATLAS_ROWS    = ANGLE_COUNT / TILES_PER_ROW;
      static constexpr int ATLAS_W       = TILE_SIZE * TILES_PER_ROW;
      static constexpr int ATLAS_H       = TILE_SIZE * ATLAS_ROWS;

      ThumbnailCache();
      ~ThumbnailCache();

      ThumbnailCache(const ThumbnailCache&) = delete;
      ThumbnailCache& operator=(const ThumbnailCache&) = delete;

      // Ensures a thumbnail for the model asset is generated (queued if needed) and
      // returns the ready atlas texture, or nullptr while it is still generating.
      SDL_GPUTexture* getModelTexture(uint64_t assetUUID);

      // Computes the hover-scrub UVs for a thumbnail occupying the screen rect
      // [rmin, rmax]: hovering and moving the cursor horizontally spins the turntable.
      void getScrubUV(const ImVec2 &rmin, const ImVec2 &rmax, ImVec2 &uv0, ImVec2 &uv1) const;

      // Drops the cached thumbnail (memory + disk) so it regenerates. Call when an
      // asset's visuals change (model reload, material edit, scale change, ...).
      void invalidate(uint64_t assetUUID);

    private:
      enum class State : uint8_t { Pending, Rendering, Ready, Failed };

      struct Entry
      {
        State state{State::Pending};
        SDL_GPUTexture* tex{nullptr};
        std::vector<uint8_t> pixels{}; // CPU atlas, only kept during generation
        int doneAngles{0};
        int loadWaits{0};              // frames spent waiting for the mesh to upload
      };

      std::unordered_map<uint64_t, Entry> entries{};
      std::deque<uint64_t> queue{};
      std::unordered_set<uint64_t> queued{};

      Renderer::Framebuffer fb{};
      Renderer::Object obj3D{};
      Renderer::Skeleton dummySkeleton;
      std::shared_ptr<Renderer::Skeleton> activeSkel{};
      uint64_t activeSkelAsset{0};
      Renderer::UniformGlobal uniGlobal{};
      Project::Object dummyObj{};
      Project::Component::Shared::MaterialInstance dummyMat{};
      bool fbReady{false};

      uint32_t passId{};
      uint64_t readbackAsset{0};
      int readbackAngle{-1};

      Entry& request(uint64_t assetUUID);

      void onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* copyPass);
      void onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& scene);
      void onPostRender(Renderer::Scene& scene);
      void renderAngle(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& scene, uint64_t assetUUID, int angle);

      std::string cachePath(uint64_t assetUUID) const;
      bool tryLoadFromDisk(uint64_t assetUUID, Entry &e);
      SDL_GPUTexture* uploadRGBA(const uint8_t* pixels, uint32_t w, uint32_t h);
  };
}
