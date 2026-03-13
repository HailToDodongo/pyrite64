/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include <renderer/material.h>

#include "lib/logger.h"

void P64::Renderer::Material::begin(Object &obj)
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

void P64::Renderer::Material::end()
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

  void set_texture(P64::Renderer::WIP_T3DMaterial *mat, rdpq_tile_t tile)
  {
    auto *tex = tile == TILE0 ? &mat->textureA : &mat->textureB;

    debugf("Tex: TILE%d A:%08X R:%08X, wh: %f %f\n", tile, tex->texAssetIdx, tex->texReference, (double)tex->s.height, (double)tex->s.low);

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

      /*if(conf && conf->tileCb) {
        conf->tileCb(conf->userData, &texParam, tile);
      }*/

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
    }
  }
}

// @TODO: temporary hack to migrate t3d:
void P64::Renderer::WIP_T3DMaterial::begin(WIP_T3DModelState &state)
{
  auto mat = this;
  if(mat->fogMode != T3D_FOG_MODE_DEFAULT && mat->fogMode != state.lastFogMode) {
    state.lastFogMode = mat->fogMode;
    t3d_fog_set_enabled(mat->fogMode == T3D_FOG_MODE_ACTIVE);
  }

  if(state.lastVertFXFunc != mat->vertexFxFunc || (
    mat->vertexFxFunc && (state.lastUvGenParams[0] != mat->textureA.texWidth || state.lastUvGenParams[1] != mat->textureA.texHeight)
  )) {
    state.lastVertFXFunc = mat->vertexFxFunc;
    state.lastUvGenParams[0] = mat->textureA.texWidth;
    state.lastUvGenParams[1] = mat->textureA.texHeight;
    t3d_state_set_vertex_fx(
      static_cast<T3DVertexFX>(state.lastVertFXFunc),
      (int16_t)state.lastUvGenParams[0],
      (int16_t)state.lastUvGenParams[1]
    );
  }

  // now apply rdpq settings, these are independent of the t3d state
  // and only need to happen before a `t3d_tri_draw` call
  if(mat->colorCombiner)
  {
    bool setCC         = mat->colorCombiner != state.lastCC;
    bool setTexture    = state.lastTextureIdxA != mat->textureA.texAssetIdx || state.lastTextureIdxB != mat->textureB.texAssetIdx;
    bool setOtherMode  = state.lastOtherMode != mat->otherModeValue || setTexture;
    bool setPrimColor  = (mat->setColorFlags & 0b001) && color_to_packed32(state.lastPrimColor) != color_to_packed32(mat->primColor);
    bool setEnvColor   = (mat->setColorFlags & 0b010) && color_to_packed32(state.lastEnvColor) != color_to_packed32(mat->envColor);
    bool setBlendColor = (mat->setColorFlags & 0b100) || (mat->otherModeValue & SOM_ALPHACOMPARE_THRESHOLD);
    setBlendColor = setBlendColor && color_to_packed32(state.lastBlendColor) != color_to_packed32(mat->blendColor);

    if(setCC || setOtherMode || setTexture) {
      rdpq_sync_pipe();
    }

    if(setTexture)
    {
      state.lastTextureIdxA = mat->textureA.texAssetIdx;
      state.lastTextureIdxB = mat->textureB.texAssetIdx;
      rdpq_sync_load();

      rdpq_tex_multi_begin();
        set_texture(mat, TILE0);
        set_texture(mat, TILE1);
      rdpq_tex_multi_end();
    }

    if(setCC) {
      state.lastCC = mat->colorCombiner;
      rdpq_mode_combiner(mat->colorCombiner);
    }

    if(setPrimColor) {
      state.lastPrimColor = mat->primColor;
      rdpq_set_prim_color(mat->primColor);
    }

    if(setBlendColor) {
      state.lastBlendColor = mat->blendColor;
      rdpq_set_blend_color(mat->blendColor);
    }

    if(setEnvColor) {
      state.lastEnvColor = mat->envColor;
      rdpq_set_env_color(mat->envColor);
    }

    if(setOtherMode) {
      __rdpq_mode_change_som(mat->otherModeMask, mat->otherModeValue);
      state.lastOtherMode = mat->otherModeValue;
    }
  }

  if(mat->renderFlags != state.lastRenderFlags) {
    t3d_state_set_drawflags(static_cast<T3DDrawFlags>(mat->renderFlags));
    state.lastRenderFlags = mat->renderFlags;
  }
}

void P64::Renderer::WIP_T3DMaterial::end(WIP_T3DModelState &state)
{
}


