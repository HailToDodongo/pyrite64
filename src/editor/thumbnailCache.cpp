/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "thumbnailCache.h"

#include <thread>
#include <filesystem>
#include <cstring>
#include <algorithm>

#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "glm/gtc/constants.hpp"

#include "../context.h"
#include "../renderer/scene.h"
#include "../renderer/n64Mesh.h"
#include "../project/assetManager.h"
#include "../utils/hash.h"
#include "../utils/fs.h"
#include "../utils/string.h"

#include "lodepng.h"

namespace fs = std::filesystem;

Editor::ThumbnailCache::ThumbnailCache()
  : dummySkeleton{ctx.gpu}
{
  passId = Utils::Hash::sha256_64bit("Editor::ThumbnailCache") & 0xFFFFFFFF;
  ctx.scene->addCopyPass(passId, [this](SDL_GPUCommandBuffer* c, SDL_GPUCopyPass* cp) { onCopyPass(c, cp); });
  ctx.scene->addRenderPass(passId, [this](SDL_GPUCommandBuffer* c, Renderer::Scene& s) { onRenderPass(c, s); });
  ctx.scene->addPostRenderCallback(passId, [this](Renderer::Scene& s) { onPostRender(s); });
}

Editor::ThumbnailCache::~ThumbnailCache()
{
  if(ctx.scene) {
    ctx.scene->removeCopyPass(passId);
    ctx.scene->removeRenderPass(passId);
    ctx.scene->removePostRenderCallback(passId);
  }
  for(auto &[uuid, e] : entries) {
    if(e.tex)SDL_ReleaseGPUTexture(ctx.gpu, e.tex);
  }
}

std::string Editor::ThumbnailCache::cachePath(uint64_t assetUUID) const
{
  return ctx.project->getPath() + "/.cache/thumbnails/" + Utils::toHex64(assetUUID) + ".png";
}

SDL_GPUTexture* Editor::ThumbnailCache::uploadRGBA(const uint8_t* pixels, uint32_t w, uint32_t h)
{
  SDL_GPUTextureCreateInfo ci{};
  ci.type = SDL_GPU_TEXTURETYPE_2D;
  ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ci.width = w;
  ci.height = h;
  ci.layer_count_or_depth = 1;
  ci.num_levels = 1;
  ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture* tex = SDL_CreateGPUTexture(ctx.gpu, &ci);
  if(!tex)return nullptr;

  SDL_GPUTransferBufferCreateInfo tbci{};
  tbci.size = w * h * 4;
  tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(ctx.gpu, &tbci);

  void* map = SDL_MapGPUTransferBuffer(ctx.gpu, tb, false);
  std::memcpy(map, pixels, w * h * 4);
  SDL_UnmapGPUTransferBuffer(ctx.gpu, tb);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(ctx.gpu);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);

  SDL_GPUTextureTransferInfo srcInfo{};
  srcInfo.transfer_buffer = tb;
  srcInfo.pixels_per_row = w;
  srcInfo.rows_per_layer = h;

  SDL_GPUTextureRegion dstRegion{};
  dstRegion.texture = tex;
  dstRegion.w = w;
  dstRegion.h = h;
  dstRegion.d = 1;

  SDL_UploadToGPUTexture(cp, &srcInfo, &dstRegion, false);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_ReleaseGPUTransferBuffer(ctx.gpu, tb);

  return tex;
}

bool Editor::ThumbnailCache::tryLoadFromDisk(uint64_t assetUUID, Entry &e)
{
  auto path = cachePath(assetUUID);
  if(!fs::exists(path))return false;

  // Invalidate if the source model is newer than the cached thumbnail.
  auto asset = ctx.project->getAssets().getEntryByUUID(assetUUID);
  if(asset && Utils::FS::getFileAge(asset->path) >= Utils::FS::getFileAge(path)) {
    return false;
  }

  std::vector<uint8_t> px{};
  unsigned w = 0, h = 0;
  if(lodepng::decode(px, w, h, path) != 0)return false;
  if(w != ATLAS_W || h != ATLAS_H)return false;

  e.tex = uploadRGBA(px.data(), w, h);
  return e.tex != nullptr;
}

Editor::ThumbnailCache::Entry& Editor::ThumbnailCache::request(uint64_t assetUUID)
{
  auto it = entries.find(assetUUID);
  if(it != entries.end())return it->second;

  Entry e{};
  if(tryLoadFromDisk(assetUUID, e)) {
    e.state = State::Ready;
  } else {
    e.state = State::Pending;
    e.pixels.assign(static_cast<size_t>(ATLAS_W) * ATLAS_H * 4, 0);
    if(!queued.contains(assetUUID)) {
      queue.push_back(assetUUID);
      queued.insert(assetUUID);
    }
  }
  return entries.emplace(assetUUID, std::move(e)).first->second;
}

SDL_GPUTexture* Editor::ThumbnailCache::getModelTexture(uint64_t assetUUID)
{
  if(!ctx.project || assetUUID == 0)return nullptr;
  Entry &e = request(assetUUID);
  return e.state == State::Ready ? e.tex : nullptr;
}

void Editor::ThumbnailCache::invalidate(uint64_t assetUUID)
{
  auto it = entries.find(assetUUID);
  if(it != entries.end()) {
    if(it->second.tex)SDL_ReleaseGPUTexture(ctx.gpu, it->second.tex);
    entries.erase(it);
  }

  queued.erase(assetUUID);
  std::erase(queue, assetUUID);

  if(readbackAsset == assetUUID)readbackAngle = -1;
  if(activeSkelAsset == assetUUID) {
    activeSkel = nullptr;
    activeSkelAsset = 0;
  }

  if(ctx.project) {
    std::error_code ec{};
    std::filesystem::remove(cachePath(assetUUID), ec);
  }
  // Next getModelTexture() will re-queue generation.
}

void Editor::ThumbnailCache::getScrubUV(const ImVec2 &rmin, const ImVec2 &rmax, ImVec2 &uv0, ImVec2 &uv1) const
{
  int angle = ANGLE_COUNT / 2; // default: the (near-)front view
  if(ImGui::IsMouseHoveringRect(rmin, rmax)) {
    float w = rmax.x - rmin.x;
    if(w > 0.0f) {
      float t = (ImGui::GetIO().MousePos.x - rmin.x) / w;
      angle = std::clamp(static_cast<int>(t * ANGLE_COUNT), 0, ANGLE_COUNT - 1);
    }
  }

  int col = angle % TILES_PER_ROW;
  int row = angle / TILES_PER_ROW;
  uv0 = {(float)col / TILES_PER_ROW, (float)row / ATLAS_ROWS};
  uv1 = {(float)(col + 1) / TILES_PER_ROW, (float)(row + 1) / ATLAS_ROWS};
}

void Editor::ThumbnailCache::onCopyPass(SDL_GPUCommandBuffer*, SDL_GPUCopyPass* copyPass)
{
  if(queue.empty() || !ctx.project)return;

  // Skinned models need their (bind-pose) skeleton uploaded before the render pass.
  uint64_t uuid = queue.front();
  auto asset = ctx.project->getAssets().getEntryByUUID(uuid);
  bool skinned = asset && !asset->model.t3dm.skeletons.empty();

  if(skinned) {
    if(activeSkelAsset != uuid || !activeSkel) {
      activeSkel = std::make_shared<Renderer::Skeleton>(ctx.gpu, asset->model, asset->conf.baseScale);
      activeSkelAsset = uuid;
    }
    activeSkel->update(*copyPass);
  } else {
    activeSkel = nullptr;
    activeSkelAsset = uuid;
    dummySkeleton.update(*copyPass);
  }
}

void Editor::ThumbnailCache::renderAngle(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& scene, uint64_t assetUUID, int angle)
{
  auto asset = ctx.project->getAssets().getEntryByUUID(assetUUID);
  if(!asset || !asset->mesh3D || !asset->mesh3D->isLoaded())return;

  obj3D.setMesh(asset->mesh3D);
  obj3D.uniform = {};
  obj3D.uniform.modelMat = glm::mat4(1.0f);
  obj3D.uniform.mat.flags = 0;
  obj3D.setObjectID(0);

  constexpr float POS_SCALE = 65536.0f;
  Utils::AABB aabb = asset->mesh3D->getAABB();
  glm::vec3 center = aabb.getCenter() * POS_SCALE;
  float radius = glm::length(aabb.getHalfExtend()) * POS_SCALE;
  if(radius < 0.0001f)radius = 1.0f;

  // The 8 frames span a half rotation centered on the front (+90°..-90°)
  float yaw = glm::half_pi<float>() - (float)angle * (glm::pi<float>() / (ANGLE_COUNT - 1));
  float pitch = glm::radians(-20.0f);
  glm::quat rot = glm::angleAxis(yaw, glm::vec3{0,1,0}) * glm::angleAxis(pitch, glm::vec3{1,0,0});

  float fov = glm::radians(60.0f);
  float dist = radius / std::sin(fov * 0.5f) * 0.95f; // fit bounding sphere, slight zoom-in
  glm::vec3 fwd = rot * glm::vec3{0,0,-1};
  glm::vec3 up  = rot * glm::vec3{0,1,0};
  glm::vec3 eye = center - fwd * dist;

  uniGlobal = {};
  uniGlobal.projMat = glm::perspective(fov, 1.0f, dist * 0.1f, dist + radius * 2.0f);
  uniGlobal.cameraMat = glm::lookAt(eye, center, up);
  uniGlobal.screenSize = {(float)TILE_SIZE, (float)TILE_SIZE};

  // Override scene lighting with a neutral studio setup just for this render.
  auto savedLights = scene.getLights();
  scene.clearLights();
  scene.addLight(Renderer::Light{ .color = {0.55f, 0.55f, 0.60f, 1.0f}, .type = 0 }); // ambient
  scene.addLight(Renderer::Light{
    .color = {1.0f, 0.98f, 0.92f, 1.0f},
    .dir = glm::normalize(glm::vec3{-0.4f, -0.7f, -0.55f}),
    .type = 1
  });

  fb.setClearColor({0.0f, 0.0f, 0.0f, 0.0f});

  SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
    cmdBuff, fb.getTargetInfo(), fb.getTargetInfoCount(), &fb.getDepthTargetInfo()
  );
  scene.getPipeline("n64").bind(pass);
  (activeSkel ? *activeSkel : dummySkeleton).use(pass);
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));

  std::vector<uint32_t> parts{};
  parts.reserve(asset->model.t3dm.models.size());
  for(uint32_t i = 0; i < asset->model.t3dm.models.size(); ++i)parts.push_back(i);

  obj3D.draw(pass, cmdBuff, Renderer::N64Mesh::ObjectRef{
    .partsIndices = parts,
    .model = &asset->model,
    .matInstance = &dummyMat,
    .obj = dummyObj
  });

  SDL_EndGPURenderPass(pass);

  scene.clearLights();
  for(auto &l : savedLights)scene.addLight(l);
}

void Editor::ThumbnailCache::onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& scene)
{
  if(queue.empty() || !ctx.project)return;

  uint64_t uuid = queue.front();
  auto eit = entries.find(uuid);
  if(eit == entries.end()) {
    queue.pop_front();
    queued.erase(uuid);
    return;
  }
  Entry &e = eit->second;

  auto asset = ctx.project->getAssets().getEntryByUUID(uuid);
  if(!asset || asset->type != Project::FileType::MODEL_3D || !asset->mesh3D) {
    e.state = State::Failed;
    queue.pop_front();
    queued.erase(uuid);
    return;
  }

  // The mesh uploads via a deferred copy pass, so wait until it is ready before
  // rendering any angle (otherwise the first frame would render an empty mesh).
  if(!asset->mesh3D->isLoaded()) {
    if(e.loadWaits == 0)asset->mesh3D->recreate(*ctx.scene);
    if(++e.loadWaits > 8) { // give up on empty/broken meshes instead of blocking the queue
      e.state = State::Failed;
      queue.pop_front();
      queued.erase(uuid);
    }
    return;
  }

  if(!fbReady) {
    fb.resize(TILE_SIZE, TILE_SIZE);
    fbReady = true;
  }

  e.state = State::Rendering;
  renderAngle(cmdBuff, scene, uuid, e.doneAngles);
  readbackAsset = uuid;
  readbackAngle = e.doneAngles;
}

void Editor::ThumbnailCache::onPostRender(Renderer::Scene&)
{
  if(readbackAngle < 0)return;

  uint64_t uuid = readbackAsset;
  int angle = readbackAngle;
  readbackAngle = -1;

  auto eit = entries.find(uuid);
  if(eit == entries.end())return;
  Entry &e = eit->second;

  auto tile = fb.readColorImage();
  if(tile.size() == static_cast<size_t>(TILE_SIZE) * TILE_SIZE * 4 &&
     e.pixels.size() == static_cast<size_t>(ATLAS_W) * ATLAS_H * 4)
  {
    int ox = (angle % TILES_PER_ROW) * TILE_SIZE;
    int oy = (angle / TILES_PER_ROW) * TILE_SIZE;
    for(int y = 0; y < TILE_SIZE; ++y) {
      std::memcpy(
        &e.pixels[(static_cast<size_t>(oy + y) * ATLAS_W + ox) * 4],
        &tile[static_cast<size_t>(y) * TILE_SIZE * 4],
        TILE_SIZE * 4
      );
    }
  }

  e.doneAngles++;
  if(e.doneAngles < ANGLE_COUNT)return;

  // All angles done: upload the atlas for display and persist it on disk async.
  e.tex = uploadRGBA(e.pixels.data(), ATLAS_W, ATLAS_H);
  e.state = e.tex ? State::Ready : State::Failed;

  auto path = cachePath(uuid);
  std::error_code ec{};
  fs::create_directories(fs::path(path).parent_path(), ec);

  // Make the cache self-ignoring so thumbnails never get committed, without
  // relying on the project's root .gitignore (which only new projects get).
  fs::path ignoreFile = fs::path(ctx.project->getPath()) / ".cache" / ".gitignore";
  if(!fs::exists(ignoreFile))Utils::FS::saveTextFile(ignoreFile, "*\n");

  std::thread([path, px = e.pixels]() mutable {
    lodepng::encode(path, px, ATLAS_W, ATLAS_H);
  }).detach();

  e.pixels.clear();
  e.pixels.shrink_to_fit();

  queue.pop_front();
  queued.erase(uuid);
}
