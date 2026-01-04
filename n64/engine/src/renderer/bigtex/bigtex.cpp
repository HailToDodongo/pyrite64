/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <string>

#include "bigtex.h"
#include <vector>

#include "renderer/pipelineBigTex.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

void P64::Renderer::BigTex::patchT3DM(T3DModel &model)
{
  auto pipeline = SceneManager::getCurrent().getRenderPipeline<RenderPipelineBigTex>();
  assert(pipeline);

  auto &textures = pipeline->textures;

  std::vector<T3DObject*> objects{};
  auto it = t3d_model_iter_create(&model, T3D_CHUNK_TYPE_OBJECT);
  while(t3d_model_iter_next(&it)) {
    objects.push_back(it.object);
  }

  uint8_t baseAddrMat = (TEX_BASE_ADDR >> 16) & 0xFF;
  for(auto &obj : objects)
  {
    auto *mat = obj->material;
    if(mat->textureA.texReference == 0xFF)continue;
    if(mat->textureA.texWidth != 256)continue;

    uint8_t matIdx = 0;
    if (mat->textureA.texReference) {
      matIdx = textures.reserveTexture();
      //debugf("Tex[%d]: <placeholder> (%s)\n", matIdx, mat->name);
    } else {
      //std::string bc1Path{mat->textureA.texPath};
      auto len = strlen(mat->textureA.texPath);
      // ends in ".sprite", replace with ".bci"
      mat->textureA.texPath[len - 6] = 'b';
      mat->textureA.texPath[len - 5] = 'c';
      mat->textureA.texPath[len - 4] = 'i';
      mat->textureA.texPath[len - 3] = '\0';

      matIdx = textures.addTexture(mat->textureA.texPath);
      //debugf("Tex[%d]: %s (%s)\n", matIdx, mat->textureA.texPath, mat->name);
    }

    mat->otherModeMask |= SOM_SAMPLE_MASK;
    mat->otherModeValue |= SOM_SAMPLE_POINT;

    // Override material for UV texture gradients
    mat->renderFlags &= ~T3D_FLAG_SHADED;
    mat->renderFlags |= T3D_FLAG_NO_LIGHT;
    mat->textureA.texPath = nullptr;
    mat->textureB.texPath = nullptr;
    mat->textureA.texReference = 0xFF;
    mat->textureB.texReference = 0xFF;

    mat->primColor = {(uint8_t)(baseAddrMat + matIdx),0,0,0xFF};
    mat->colorCombiner = RDPQ_COMBINER2(
      (1, 0, TEX0, TEX1),     (0,0,0,1),
      (1, 0, PRIM, COMBINED), (0,0,0,1)
    );

    ++matIdx;
  }

  rspq_block_begin();
    auto t3dState = t3d_model_state_create();
    for(auto obj : objects) {
      t3d_model_draw_material(obj->material, &t3dState);
      t3d_model_draw_object(obj, nullptr);
    }
    t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0,0);
  model.userBlock = rspq_block_end();
/*
  rspq_block_begin();
    rdpq_sync_pipe();
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
