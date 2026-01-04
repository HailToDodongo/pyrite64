/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "renderer/particles/ptxSprites.h"
#include "debug/debugDraw.h"
#include "lib/logger.h"

namespace
{
  constexpr float BASE_SCALE = 1.0f;
  constexpr float BASE_SCALE_INV = 1.0f / BASE_SCALE;
  constexpr fm_vec3_t BASE_SCALE_VEC_INV{BASE_SCALE_INV, BASE_SCALE_INV, BASE_SCALE_INV};

  T3DMat4 TMP_MAT{{
    {BASE_SCALE_INV, 0, 0, 0},
    {0, BASE_SCALE_INV, 0, 0},
    {0, 0, BASE_SCALE_INV, 0},
    {0,0,0,1}
  }};

  // compare op for fm_vec3_t
  /*constexpr bool operator==(const fm_vec3_t &a, const fm_vec3_t &b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }*/
}

P64::PTX::Sprites::Sprites(const char* spritePath, const Conf &conf_)
  : system{System::TEX_A_S16, conf_.maxSize}, conf{conf_}
{
  sprite = sprite_load(spritePath);
  system.count = 0;
  system.pos = {-999,0,0}; // forces matrix creation for 0,0,0

  rspq_block_begin();
  {
    rdpq_mode_begin();
      if(conf.isRotating) {
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_alphacompare(64);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_zbuf(true, false);
      } else {
        rdpq_mode_filter(FILTER_POINT);
        rdpq_mode_alphacompare(10);
      }
      rdpq_mode_combiner(RDPQ_COMBINER1((PRIM,0,TEX0,0), (TEX0,0,ENV,0)));
    rdpq_mode_end();

    int shift =  - __builtin_ctz(sprite->height / 8);
    rdpq_texparms_t p{};
    p.s = {.translate = 0, .scale_log = shift, .repeats = REPEAT_INFINITE, .mirror = conf.isRotating != 0};
    p.t = {.translate = 0, .scale_log = shift, .repeats = REPEAT_INFINITE, .mirror = conf.isRotating != 0};
    rdpq_sprite_upload(TILE0, sprite, &p);

    tpx_state_set_scale(1.0f, 1.0f);
  }
  setupDPL = rspq_block_end();
  mirrorPt = 32;
  if(!conf.isRotating)mirrorPt = 0;
}

P64::PTX::Sprites::~Sprites() {
  rspq_block_free(setupDPL);
  sprite_free(sprite);
}

void P64::PTX::Sprites::add(const fm_vec3_t &pos, uint32_t seed, color_t col, float scale)
{
  if(system.isFull()) {
    Log::warn("system full, cannot add more sprites!\n");
    return;
  }

  auto posScaled = pos * BASE_SCALE;

  uint32_t offset = seed;
  if(!conf.noRng) {
    seed = (seed * 23) >> 3;
    offset = (seed * 23) % 7;
    offset *= sprite->height;
  }

  auto buff = system.getBufferS16();
  auto p = tpx_buffer_s16_get_pos(buff, system.count);
  p[0] = (int16_t)posScaled.x;
  p[1] = (int16_t)posScaled.y;
  p[2] = (int16_t)posScaled.z;

  *tpx_buffer_s16_get_size(buff, system.count) = (int8_t)(scale * 120.0f);
  *tpx_buffer_s16_get_tex_offset(buff, system.count) = offset;

  auto c = tpx_buffer_s16_get_rgba(buff, system.count);
  c[0] = col.r;
  c[1] = col.g;
  c[2] = col.b;
  c[3] = 0xFF;

  ++system.count;
}

void P64::PTX::Sprites::draw(float deltaTime) {
  animTimer += deltaTime * 15.0f;
  int16_t uvOffset = (int16_t)(animTimer);
  if(uvOffset >= 8)animTimer -= 8.0f;

  rspq_block_run(setupDPL);
  tpx_state_set_tex_params(uvOffset * (1024/sprite->height), mirrorPt);

  system.draw();
}

void P64::PTX::Sprites::clear() {
  system.count = 0;
}


