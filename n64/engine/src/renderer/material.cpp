/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include <renderer/material.h>

#include "lib/logger.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

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
    params.s.translate = static_cast<float>(tile.s.offset) * (1.0f / 64.0f);
    params.s.scale_log = tile.s.scale;
    params.s.repeats = static_cast<float>(tile.s.repeat) * (1.0f / 16.0f);
    params.s.mirror = tile.s.mirror;

    params.t.translate = static_cast<float>(tile.t.offset) * (1.0f / 64.0f);
    params.t.scale_log = tile.t.scale;
    params.t.repeats = static_cast<float>(tile.t.repeat) * (1.0f / 16.0f);
    params.t.mirror = tile.t.mirror;
    return params;
  }
}

P64::Renderer::MaterialInstance::~MaterialInstance()
{
  for(int s=0; s<MAX_SLOTS; ++s)
  {
    if(setMask & (1 << (MAX_SLOTS + s)))
    {
      if(texSlots[s].block[0])rspq_block_free(texSlots[s].block[0]);
      if(texSlots[s].block[1])rspq_block_free(texSlots[s].block[1]);
      if(texSlots[s].block[2])rspq_block_free(texSlots[s].block[2]);
    }
  }
}

void P64::Renderer::MaterialInstance::begin(Object &obj)
{
  if(!doesAnything())return;

  if(setMask & MASK_DEPTH) {
    rdpq_mode_push();
    rdpq_mode_zbuf(getDepthRead(), getDepthWrite());
  }
  if(setMask & MASK_PRIM) {
    rdpq_set_prim_color(colorPrim);
  }
  if(setMask & MASK_ENV) {
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

  //debugf("MaterialInstance begin: setMask=%04X (%d)\n", setMask, setsSlots());
  if(setsSlots())
  {
    for(uint32_t s=0; s<MAX_SLOTS; ++s)
    {
      if(setMask & (1 << (MAX_SLOTS + s)))
      {
        auto &slot = texSlots[s];
        auto params = unpackTile(slot.tile);

        /*debugf("MaterialInstance: setting slot %lu, assetIdx=%04X, phIndex=%d, phType=%d\n",
          s, slot.tile.texAssetIdx, slot.tile.phIndex, (int)slot.tile.phType
        );*/

        auto tmp = slot.block[0];
        slot.block[0] = slot.block[2];
        slot.block[2] = slot.block[1];
        slot.block[1] = tmp;

        rspq_block_begin_reuse(slot.block[0]);

          auto rdpTile = (rdpq_tile_t)texSlots[s].tile.phIndex;
          if(slot.tile.phType == Material::Tile::PlaceholderType::FULL)
          {
            auto tex =  (sprite_t*)AssetManager::getByIndex(slot.tile.texAssetIdx);
            rdpq_sprite_upload(rdpTile, tex, &params);
          } else {
            // Note: for tile placeholders, "repeats" stores the texture size
            rdpq_set_tile_size(TILE0,
              params.s.translate, params.t.translate,
              params.s.translate + params.s.repeats,
              params.t.translate + params.t.repeats
            );
          }

        slot.block[0] = rspq_block_end();
        rspq_block_set_ph((rspq_block_t*)s, slot.block[0]);
      }
    }
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
    rdpq_mode_pop();
  }
}

void P64::Renderer::Material::begin(MaterialState &state)
{
  state = {};
  DynamicData ptr{data};
  uint8_t t3dVertFxFunc{};
  uint16_t t3dVertFxArg0{};
  uint16_t t3dVertFxArg1{};

  /*debugf("Set flags: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
    sets(FLAG_OVERRIDE) ? "OVERRIDE" : "",  sets(FLAG_TEX0) ? "TEX0" : "",
    sets(FLAG_TEX1) ? "TEX1" : "",          sets(FLAG_CC) ? "CC" : "",
    sets(FLAG_BLENDER) ? "BLENDER" : "",    sets(FLAG_FOG) ? "FOG" : "",
    sets(FLAG_PRIM) ? "PRIM" : "",          sets(FLAG_ENV) ? "ENV" : "",
    sets(FLAG_ZPRIM) ? "ZPRIM" : "",        sets(FLAG_T3D_VERT_FX) ? "T3D_VERT_FX" : "",
    sets(FLAG_ALPHA_COMP) ? "A-COMP" : "",  sets(FLAG_K4K5) ? "K4K5" : "",
    sets(FLAG_PRIMLOD) ? "PRIMLOD" : "",    sets(FLAG_AA) ? "AA" : "",
    sets(FLAG_DITHER) ? "DITHER" : "",      sets(FLAG_FILTER) ? "FILTER" : "",
    sets(FLAG_ZMODE) ? "ZMODE" : "",        sets(FLAG_PERSP) ? "PERSP" : ""
  );*/

  if(sets(FLAG_OVERRIDE)) {
    rdpq_mode_push();
  }
  rdpq_mode_begin();

  if(sets(FLAG_TEX0) || sets(FLAG_TEX1))
  {
    bool setsBothTex = sets(FLAG_TEX0) && sets(FLAG_TEX1);
    if(setsBothTex)rdpq_tex_multi_begin();

    uint16_t lastTexIdx = 0xFFFF;

    auto handleTexture = [&](rdpq_tile_t rdpTile)
    {
      auto &tile = ptr.fetch<Tile>();
      auto params = unpackTile(tile);

      //debugf("Texture[%d]: assetIdx=%04X, phIndex=%d, phType=%d\n", rdpTile, tile.texAssetIdx, tile.phIndex, (int)tile.phType);

      if(tile.phType == Tile::PlaceholderType::FULL) {
        rspq_block_run((rspq_block_t*)(uint32_t)tile.phIndex);
      } else {

        if(lastTexIdx != tile.texAssetIdx) {
          auto sprite = (sprite_t*)AssetManager::getByIndex(tile.texAssetIdx);
          assert(sprite);
          rdpq_sprite_upload(rdpTile, sprite, &params);
          lastTexIdx = tile.texAssetIdx;
        } else {
          rdpq_tex_reuse(rdpTile, &params);
        }

        if(tile.phType == Tile::PlaceholderType::TILE) {
          rspq_block_run((rspq_block_t*)(uint32_t)tile.phIndex);
        }
      }
    };

    if(sets(FLAG_TEX0))handleTexture(TILE0);
    if(sets(FLAG_TEX1))handleTexture(TILE1);

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


