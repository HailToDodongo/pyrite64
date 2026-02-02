/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "dropShadows.h"

#include <assets/assetManager.h>
#include <t3d/t3d.h>

#include "../../p64/assetTable.h"
#include "lib/matrixManager.h"
#include "debug/debugDraw.h"

namespace
{
  constexpr int MAX_SHADOWS = 16;
  constexpr int BUFFERS = 3;

  constexpr int MAX_SHADOWS_VERTICES = MAX_SHADOWS * 4;
  constexpr uint32_t MAX_VERTICES = T3D_VERTEX_CACHE_SIZE & ~0b11; // must be multiple of 4

  uint32_t shadowCount = 0;
  uint32_t vertOffset = 0;
  uint32_t frameIdx = 0;

  T3DVertPacked *vertices{};
  T3DVertPacked *currVertBuff{};

  sprite_t *shadowTex{};
  rspq_block_t *setupDPL{};
}

namespace P64::User::DropShadows
{
  void init()
  {
    shadowCount = 0;
    vertOffset = 0;
    vertices = static_cast<T3DVertPacked*>(malloc_uncached(
      sizeof(T3DVertPacked) * MAX_SHADOWS_VERTICES / 2 * BUFFERS
    ));
    currVertBuff = vertices;

    // prefill static data (UVs)
    constexpr int16_t UV_END = 64 << 5;
    for(int i = 0; i < MAX_SHADOWS_VERTICES/4*BUFFERS; i++) {
      auto *uvA = t3d_vertbuffer_get_uv(vertices, i * 4 + 2);
      auto *uvB = t3d_vertbuffer_get_uv(vertices, i * 4 + 3);
      auto *uvC = t3d_vertbuffer_get_uv(vertices, i * 4 + 1);
      auto *uvD = t3d_vertbuffer_get_uv(vertices, i * 4 + 0);
      uvA[0] = 0;      uvA[1] = 0;
      uvB[0] = UV_END; uvB[1] = 0;
      uvC[0] = UV_END; uvC[1] = UV_END;
      uvD[0] = 0;      uvD[1] = UV_END;

      *t3d_vertbuffer_get_color(vertices, i*4+2) = 0xFFFFFFFF;
      *t3d_vertbuffer_get_color(vertices, i*4+3) = 0xFFFFFFFF;
      *t3d_vertbuffer_get_color(vertices, i*4+1) = 0xFFFFFFFF;
      *t3d_vertbuffer_get_color(vertices, i*4+0) = 0xFFFFFFFF;
    }

    shadowTex = (sprite_t*)AssetManager::getByIndex("shadow.i8.sprite"_asset);
    assert(shadowTex);

    rspq_block_begin();
      rdpq_sync_pipe();
      rdpq_mode_push();

      rdpq_mode_begin();
        rdpq_mode_zbuf(true, false);
        //rdpq_mode_zmode(ZMODE_DECAL);
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_alphacompare(40);
        rdpq_mode_fog(0);
      rdpq_mode_end();

    rdpq_set_prim_color({0, 0, 0, 0x70});
    rdpq_sync_tile();
    rdpq_sync_load();

    rdpq_texparms_t tp{};
    tp.t.mirror = tp.s.mirror = true;
    tp.t.repeats = tp.s.repeats = 2.0f;
    rdpq_sprite_upload(TILE0, shadowTex, &tp);

    t3d_fog_set_enabled(false);
    t3d_state_set_drawflags((T3DDrawFlags)(T3D_FLAG_DEPTH | T3D_FLAG_TEXTURED | T3D_FLAG_CULL_BACK));
    t3d_state_set_vertex_fx(T3D_VERTEX_FX_NONE, 0, 0);
    setupDPL = rspq_block_end();
  }

  void destroy() {
    free_uncached(vertices);
    rspq_block_free(setupDPL);
  }

  void addShadow(const fm_vec3_t &pos_, const fm_vec3_t &normal, float size, float strength) {
    size *= 50;

    auto pos = pos_ + normal*2;

    if (shadowCount < MAX_SHADOWS) {
      auto posA = t3d_vertbuffer_get_pos(currVertBuff, vertOffset+2);
      auto posB = t3d_vertbuffer_get_pos(currVertBuff, vertOffset+3);
      auto posC = t3d_vertbuffer_get_pos(currVertBuff, vertOffset+1);
      auto posD = t3d_vertbuffer_get_pos(currVertBuff, vertOffset+0);

      // flat floors are very likely, so we can optimize this case
      if(normal.y > 0.99f) {
        posA[0] = pos.x - size;
        posA[1] = pos.y;
        posA[2] = pos.z - size;

        posB[0] = pos.x + size;
        posB[1] = pos.y;
        posB[2] = pos.z - size;

        posC[0] = pos.x + size;
        posC[1] = pos.y;
        posC[2] = pos.z + size;

        posD[0] = pos.x - size;
        posD[1] = pos.y;
        posD[2] = pos.z + size;
      } else {
        // ...otherwise we need to calculate the vectors to make a shadow plane
        fm_vec3_t right, up;
        t3d_vec3_cross(right, normal, {0,1,0});
        t3d_vec3_cross(up, right, normal);
        t3d_vec3_norm(&right);
        t3d_vec3_norm(&up);

        auto vecA = (right - up) * size;
        auto vecB = (right + up) * size;
        auto p0 = pos + vecA;
        auto p1 = pos + vecB;
        auto p2 = pos - vecA;
        auto p3 = pos - vecB;

/*
        Debug::drawLine(posScaled, p0, {0xFF, 0x00, 0x00, 0xFF});
        Debug::drawLine(posScaled, p1, {0x00, 0xFF, 0x00, 0xFF});
        Debug::drawLine(posScaled, p2, {0x00, 0x00, 0xFF, 0xFF});
        Debug::drawLine(posScaled, p3, {0xFF, 0x00, 0xFF, 0xFF});
*/
        for(int i=0; i<3; ++i) {
          posA[i] = p0.v[i];
          posB[i] = p1.v[i];
          posC[i] = p2.v[i];
          posD[i] = p3.v[i];
        }
      }

      ++shadowCount;
      vertOffset += 4;
    }
  }

  void draw() {

    t3d_matrix_pop(1); // go back to camera
    rspq_block_run(setupDPL);
    uint32_t count = shadowCount * 4;
    auto vert = currVertBuff;

    t3d_state_set_depth_offset(-40);
    while(count)
    {
      uint32_t localCount = count;
      if(localCount > MAX_VERTICES)localCount = MAX_VERTICES;

      t3d_vert_load(vert, 0, localCount);
      t3d_quad_draw_unindexed(0, localCount / 4);
      vert += localCount / 2;
      count -= localCount;
    }
    t3d_state_set_depth_offset(0);
    t3d_matrix_push_pos(1);

    rdpq_sync_pipe();
    rdpq_mode_pop();
  }

  void reset() {
    shadowCount = 0;
    vertOffset = 0;
    frameIdx = (frameIdx + 1) % BUFFERS;
    currVertBuff = &vertices[(MAX_SHADOWS_VERTICES / 2) * frameIdx];
  }
}