/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include <renderer/material.h>

#include "lib/logger.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

void P64::Renderer::MaterialInstance::begin(Object &obj)
{
  if(!doesAnything())return;

  if(setMask & MASK_DEPTH) {
    rdpq_sync_pipe();
    rdpq_mode_push();
    rdpq_mode_zbuf(getDepthRead(), getDepthWrite());
  }
  if(setMask & MASK_PRIM) {
    rdpq_set_prim_color(colorPrim);
  }
  if(setMask & MASK_ENV) {
    rdpq_sync_pipe();
    rdpq_set_env_color(colorEnv);
  }
  if(fresnel != 0)
  {
    auto &scene = obj.getScene();
    auto &cam = scene.getActiveCamera();
    auto &light = scene.startLightingOverride(true);
    light.reset();

    for(int i=0; i<fresnel; ++i) {
      light.addPointLight(
        colorFresnel,
        cam.getPos() + fm_vec3_t{2.0f, 2.0f, 2.0f},
        10000.0f
      );
    }

    light.apply();
  }
}

void P64::Renderer::MaterialInstance::end()
{
  if(fresnel != 0)
  {
    SceneManager::getCurrent().endLightingOverride();
  }
  if(setMask & MASK_DEPTH)
  {
    rdpq_sync_pipe();
    rdpq_mode_pop();
  }
}

namespace
{
  struct DynamicData
  {
    char* data{};

    template<typename T>
    const T& fetch() {
      auto res = (T*)data;
      data += sizeof(T);
      return *res;
    }
  };

  constexpr rdpq_texparms_t unpackTile(const P64::Renderer::Material::Tile &tile)
  {
    rdpq_texparms_t params{};
    params.s.translate = static_cast<float>(tile.s.offset) * (1.0f / 8.0f);
    params.s.scale_log = tile.s.scale;
    params.s.repeats = static_cast<float>(tile.s.repeat) * (1.0f / 16.0f);
    params.s.mirror = tile.s.mirror;

    params.t.translate = static_cast<float>(tile.t.offset) * (1.0f / 8.0f);
    params.t.scale_log = tile.t.scale;
    params.t.repeats = static_cast<float>(tile.t.repeat) * (1.0f / 16.0f);
    params.t.mirror = tile.t.mirror;
    return params;
  }
}

void P64::Renderer::Material::begin(MaterialState &state)
{
  state = {};
  DynamicData ptr{data};
  uint8_t t3dVertFxFunc{};
  uint16_t t3dVertFxArg0{};
  uint16_t t3dVertFxArg1{};

  if(sets(FLAG_OVERRIDE)) {
    rdpq_mode_push();
  }

  rdpq_mode_begin();

  if(sets(FLAG_TEX0) || sets(FLAG_TEX1))
  {
    bool setsBothTex = sets(FLAG_TEX0) && sets(FLAG_TEX1);
    if(setsBothTex)rdpq_tex_multi_begin();

    uint16_t lastTexIdx = 0xFFFF;

    if(sets(FLAG_TEX0))
    {
      auto &tile = ptr.fetch<Tile>();
      auto params = unpackTile(tile);
      lastTexIdx = tile.texAssetIdx;
      auto sprite = (sprite_t*)AssetManager::getByIndex(tile.texAssetIdx);
      rdpq_sprite_upload(TILE0, sprite, &params);
      assert(sprite);
    }
    if(sets(FLAG_TEX1))
    {
      auto &tile = ptr.fetch<Tile>();
      auto params = unpackTile(tile);

      if(lastTexIdx != tile.texAssetIdx)
      {
        auto sprite = (sprite_t*)AssetManager::getByIndex(tile.texAssetIdx);
        assert(sprite);
        rdpq_sprite_upload(TILE1, sprite, &params);
      } else {
        rdpq_tex_reuse(TILE1, &params);
      }
    }
    if(setsBothTex)rdpq_tex_multi_end();
  }

  if(sets(FLAG_CC)) {
    rdpq_mode_combiner(ptr.fetch<rdpq_combiner_t>());
  }
  if(sets(FLAG_BLENDER)) {
    rdpq_mode_blender(ptr.fetch<rdpq_blender_t>());
  }
  if(sets(FLAG_FOG)) {
    rdpq_mode_fog(ptr.fetch<rdpq_blender_t>());
  }
  if(sets(FLAG_PRIM)) {
    rdpq_set_prim_color(ptr.fetch<color_t>());
  }
  if(sets(FLAG_ENV)) {
    rdpq_set_env_color(ptr.fetch<color_t>());
  }
  if(sets(FLAG_ZPRIM)) {
    const auto zPrim = ptr.fetch<int16_t>();
    const auto zDelta = ptr.fetch<int16_t>();
    rdpq_mode_zoverride(true, zPrim, zDelta);
  }

  if(sets(FLAG_T3D_VERT_FX)) {
    t3dVertFxArg0 = ptr.fetch<uint16_t>();
    t3dVertFxArg1 = ptr.fetch<uint16_t>();
    t3dVertFxFunc = ptr.fetch<uint8_t>();
  }

  if(sets(FLAG_ALPHA_COMP)) {
    rdpq_mode_alphacompare(ptr.fetch<uint8_t>());
  }
  if(sets(FLAG_K4K5)) {
    const auto k4 = ptr.fetch<uint8_t>();
    const auto k5 = ptr.fetch<uint8_t>();
    rdpq_set_yuv_parms(0, 0, 0, 0, k4, k5);
  }
  if(sets(FLAG_PRIMLOD)) {
    rdpq_set_prim_lod_frac(ptr.fetch<uint8_t>());
  }
  if(sets(FLAG_AA)) {
    rdpq_mode_antialias(getAA());
  }
  if(sets(FLAG_DITHER)) {
    rdpq_mode_dithering(getDither());
  }
  if(sets(FLAG_FILTER)) {
    rdpq_mode_filter(getFilter());
  }
  if(sets(FLAG_ZMODE)) {
    rdpq_mode_zbuf(getZRead(), getZWrite());
  }
  if(sets(FLAG_PERSP)) {
    rdpq_mode_persp(getModePersp());
  }

  rdpq_mode_end();

  // t3d calls should be at the end to avoid needless ucode switches,
  // after the material t3d calls for the mesh follow
  if(sets(FLAG_T3D_VERT_FX)) {
    t3d_state_set_vertex_fx(
      static_cast<T3DVertexFX>(t3dVertFxFunc),
      t3dVertFxArg0, t3dVertFxArg1
    );
  }

  // @TODO: optimize to u8
  t3d_state_set_drawflags(static_cast<T3DDrawFlags>(t3dDrawFlags));
}

void P64::Renderer::Material::end(MaterialState &state)
{
  if(sets(FLAG_T3D_VERT_FX)) {
    t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0, 0);
  }
  if(sets(FLAG_OVERRIDE)) {
    rdpq_mode_pop();
  }
}

const P64::Renderer::Material::Tile* P64::Renderer::Material::getTile(uint8_t idx)
{
  assert(idx < 2);

  DynamicData ptr{data};
  if(sets(FLAG_TEX0)) {
    if(idx == 0)return &ptr.fetch<Tile>();
    ptr.fetch<Tile>(); // skip
  }
  if(sets(FLAG_TEX1)) {
    if(idx == 1)return &ptr.fetch<Tile>();
    ptr.fetch<Tile>(); // skip
  }
  return nullptr;
}


