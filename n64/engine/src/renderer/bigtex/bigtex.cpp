/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <string>

#include "bigtex.h"
#include <vector>

#include "renderer/material.h"
#include "renderer/pipelineBigTex.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

void P64::Renderer::BigTex::patchT3DM(T3DModel &model)
{
  if(model.userBlock)return; // already processed
  auto pipeline = SceneManager::getCurrent().getRenderPipeline<RenderPipelineBigTex>();
  assert(pipeline);

  auto &textures = pipeline->textures;
  constexpr uint8_t baseAddrMat = (TEX_BASE_ADDR >> 16) & 0xFF;
  uint8_t matIdx = 0;
  MaterialState state{};

  rspq_block_begin();

  auto it = t3d_model_iter_create(&model, T3D_CHUNK_TYPE_OBJECT);
  while(t3d_model_iter_next(&it))
  {
    if(it.object->userBlock)return; // already processed

    auto *mat = (P64::Renderer::Material*)it.object->material;
    auto *tile = mat->getTile(0);
    if(tile)
    {
      if (tile->isPlaceholder()) {
        matIdx = textures.reserveTexture();
        //debugf("Tex[%d]: <placeholder> (%s)\n", matIdx, mat->name);
      } else {
        matIdx = textures.addTexture(
          AssetManager::getPathByIndex(tile->texAssetIdx)
        );
      }
    }

    mat->flagsData = Material::FLAG_T3D_VERT_FX;
    mat->t3dDrawFlags &= ~T3D_FLAG_SHADED;
    rdpq_set_prim_color({(uint8_t)(baseAddrMat + matIdx), 0, 0, 0xFF});

    mat->begin(state);
    t3d_model_draw_object(it.object, nullptr);
    mat->end(state);
  }

  model.userBlock = rspq_block_end();
/*
  rspq_block_begin();
    rdpq_mode_combiner(RDPQ_COMBINER1((1, SHADE, PRIM, 0), (0,0,0,1)));
    rdpq_mode_blender(0);
    rdpq_mode_alphacompare(0);

    rdpq_set_prim_color({0xFF, 0xFF, 0xFF, 0xFF});
    t3d_state_set_drawflags((T3DDrawFlags)(T3D_FLAG_DEPTH | T3D_FLAG_SHADED | T3D_FLAG_CULL_BACK));
    t3d_light_set_count(1);

    int lastNoDepth = -1;
    for(auto obj : objects) {
        t3d_model_draw_object(obj, nullptr);
    }
  dplDrawShade = rspq_block_end();
  */
}
