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
  inline bool is_power_of_two(uint16_t x) {
    return (x & (x - 1)) == 0;
  }

  void set_texture(P64::Renderer::Material *mat, rdpq_tile_t tile)
  {
    /*auto *tex = tile == TILE0 ? &mat->textureA : &mat->textureB;

    //P64::Log::info("Tex: TILE%d A:%08X R:%08X, wh: %d %d", tile, tex->texAssetIdx, tex->texReference, tex->texWidth, tex->texHeight);

    if(tex->texAssetIdx != 0xFFFF || tex->texReference)
    {
      if(tex->texAssetIdx != 0xFFFF && !tex->texture) {
        tex->texture = static_cast<sprite_t*>(
          P64::AssetManager::getByIndex(tex->texAssetIdx)
        );
      }

      rdpq_texparms_t texParam = (rdpq_texparms_t){};
      texParam.s.translate = tex->s.low;
      texParam.s.mirror = tex->s.mirror;
      texParam.s.repeats = REPEAT_INFINITE;
      texParam.s.scale_log = (int)tex->s.shift;

      if(tex->s.clamp) {
        if(is_power_of_two(tex->texWidth)) {
          texParam.s.repeats = (tex->s.height+1.0f) / (float)tex->texWidth;
        } else {
          texParam.s.repeats = 1;
        }
      }

      texParam.t.translate = tex->t.low;
      texParam.t.mirror = tex->t.mirror;
      texParam.t.repeats = REPEAT_INFINITE;
      texParam.t.scale_log = (int)tex->t.shift;

      if(tex->t.clamp) {
        if(is_power_of_two(tex->texHeight)) {
          texParam.t.repeats = (tex->t.height+1.0f) / (float)tex->texHeight;
        } else {
          texParam.t.repeats = 1;
        }
      }



      // @TODO: don't upload texture if only the tile settings differ
      if(tex->texReference) {
        P64::Log::error("Dynamic textures not supported yet! (%08X | %08X/%08X)", tex->texReference, mat->textureA.texAssetIdx, mat->textureB.texAssetIdx);
        //assertf(false, "Dynamic textures not supported yet");
        //if(conf && conf->dynTextureCb)conf->dynTextureCb(conf->userData, mat, &texParam, tile);
      } else {
        P64::Log::info("Load texture: %d\n", tex->texAssetIdx);
        rdpq_sync_tile();
        if(tile == TILE1 && mat->textureA.texAssetIdx == mat->textureB.texAssetIdx) {
          rdpq_tex_reuse(TILE1, &texParam);
        } else {
          rdpq_sprite_upload(tile, tex->texture, &texParam);
        }
      }
    }*/
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
}

// @TODO: temporary hack to migrate t3d:
void P64::Renderer::Material::begin(WIP_T3DModelState &state)
{
  state = {};
  DynamicData ptr{data};

  if(sets(FLAG_OVERRIDE)) {
    rdpq_mode_push();
  }

  rdpq_mode_begin();

  if(sets(FLAG_TEX0)) {
    auto &tile = ptr.fetch<Tile>();
    // @TODO:
  }
  if(sets(FLAG_TEX1)) {
    auto &tile = ptr.fetch<Tile>();
    // @TODO:
  }
  debugf("FLAGS: %08lX\n", flagsData);

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

  if(sets(FLAG_T3D_VERT_FX)) {
    /*const auto fn = ptr.fetch<int16_t>();
    const auto arg0 = ptr.fetch<int16_t>();
    const auto arg1 = ptr.fetch<int16_t>();
    t3d_state_set_vertex_fx(static_cast<T3DVertexFX>(fn), arg0, arg1);
    */
    assertf(false, "@TODO: FLAG_T3D_VERT_FX");
  }

  // @TODO: optimize to u8
  t3d_state_set_drawflags(static_cast<T3DDrawFlags>(t3dDrawFlags));
}

void P64::Renderer::Material::end(WIP_T3DModelState &state)
{
  if(sets(FLAG_OVERRIDE)) {
    rdpq_mode_pop();
  }
}


