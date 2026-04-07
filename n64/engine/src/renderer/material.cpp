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
    if(!setsPlaceholder(s))continue;
    if(placeholders[s].block[0])rspq_block_free(placeholders[s].block[0]);
    if(placeholders[s].block[1])rspq_block_free(placeholders[s].block[1]);
    if(placeholders[s].block[2])rspq_block_free(placeholders[s].block[2]);
  }
}

void P64::Renderer::MaterialInstance::Placeholder::update()
{
  auto params = unpackTile(tile);

  /*debugf("MaterialInstance %p: setting slot, assetIdx=%04X, phIndex=%d, phType=%d\n",
  this, tile.texAssetIdx, tile.phIndex, (int)tile.phType
  );*/

  auto tmp = block[0];
  block[0] = block[2];
  block[2] = block[1];
  block[1] = tmp;

  //debugf("Blocks: %p %p %p\n", block[0], block[1], block[2]);

  rspq_block_begin_reuse(block[0]);

  auto rdpTile = (rdpq_tile_t)tile.phIndex;
  if(tile.phType == Material::Tile::PlaceholderType::FULL)
  {
    auto tex =  (sprite_t*)AssetManager::getByIndex(tile.texAssetIdx);
    rdpq_sprite_upload(rdpTile, tex, &params);
  } else {
    // Note: for tile placeholders, "repeats" stores the texture size
    rdpq_set_tile_size(rdpTile,
      params.s.translate, params.t.translate,
      params.s.translate + params.s.repeats,
      params.t.translate + params.t.repeats
    );
  }

  block[0] = rspq_block_end();
}

void P64::Renderer::MaterialInstance::init()
{
  for(int s=0; s<MAX_SLOTS; ++s) {
    if(setsPlaceholder(s))placeholders[s].update();
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

  if(setsAnyPlaceholder()) {
    for(uint32_t s=0; s<MAX_SLOTS; ++s) {
      if(setsPlaceholder(s)) {
        //debugf("Set placeholder %lu -> %p\n", s, placeholders[s].block[0]);
        rspq_block_set_placeholder((rspq_block_t*)s, placeholders[s].block[0]);
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

  /*debugf("Mat: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
    sets(FLAG_OVERRIDE)   ? "Overrd" : "  --  ", sets(FLAG_TEX0)        ? " TEX0 " : "  --  ",
    sets(FLAG_TEX1)       ? " TEX1 " : "  --  ", sets(FLAG_CC)          ? "  CC  " : "  --  ",
    sets(FLAG_BLENDER)    ? "BLEND " : "  --  ", sets(FLAG_FOG)         ? " Fog  " : "  --  ",
    sets(FLAG_PRIM)       ? " PRIM " : "  --  ", sets(FLAG_ENV)         ? " ENV  " : "  --  ",
    sets(FLAG_ZPRIM)      ? "Z-PRIM" : "  --  ", sets(FLAG_T3D_VERT_FX) ? "VertFX" : "  --  ",
    sets(FLAG_ALPHA_COMP) ? "A-Comp" : "  --  ", sets(FLAG_K4K5)        ? " K4K5 " : "  --  ",
    sets(FLAG_PRIMLOD)    ? "P-LOD " : "  --  ", sets(FLAG_AA)          ? "  AA  " : "  --  ",
    sets(FLAG_DITHER)     ? "Dither" : "  --  ", sets(FLAG_FILTER)      ? "Filter" : "  --  ",
    sets(FLAG_ZMODE)      ? "ZMode " : "  --  ", sets(FLAG_PERSP)       ? "Persp." : "  --  "
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


