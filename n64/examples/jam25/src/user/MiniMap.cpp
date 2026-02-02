#include "script/userScript.h"
#include "systems/context.h"

namespace
{
  constexpr int MAP_DIM = 64;
  constexpr float MAP_POS_SCALE = 0.025f;

  constexpr int STRIDE = MAP_DIM;

  void setPixel(surface_t &surf, int x, int y, uint8_t color)
  {
    if(x < 0 || x >= surf.width || y < 0 || y >= surf.height)return;
    uint8_t *pix = (uint8_t*)surf.buffer;
    pix[y * STRIDE + x] = color;
  }
}

namespace P64::Script::C00EFAD26DE8842F
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Allowed types:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    surface_t mapSurf;
    float playerAngle;
    float lastPlayerPos[2];
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      surface_free(&data->mapSurf);
      return;
    }

    data->playerAngle = 0;
    data->mapSurf = surface_alloc(FMT_I8, MAP_DIM, MAP_DIM);
    assert(STRIDE == data->mapSurf.stride);
    sys_hw_memset16(data->mapSurf.buffer, 0x0000, data->mapSurf.height * STRIDE);
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto player = SceneManager::getCurrent().getObjectById(User::ctx.controlledId);
    if(!player)return;

    auto &cam = SceneManager::getCurrent().getActiveCamera();
    auto viewDir = cam.getViewDir();

    data->playerAngle = atan2f(viewDir.x, viewDir.z);

    data->lastPlayerPos[0] = (player->pos.x * MAP_POS_SCALE) + MAP_DIM/2;
    data->lastPlayerPos[1] = (player->pos.z * MAP_POS_SCALE) + MAP_DIM/2;

    int mapPosX = (int)(data->lastPlayerPos[0]);
    int mapPosY = (int)(data->lastPlayerPos[1]);

    for(int y=0; y<3; ++y)
    {
      for(int x=0; x<3; ++x)
      {
        setPixel(data->mapSurf, mapPosX + x - 1, mapPosY + y - 1, 0x44);
      }
    }
    setPixel(data->mapSurf, mapPosX, mapPosY, 0xFF);

  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    rdpq_blitparms_t p{};
    p.theta = -data->playerAngle;
    p.cx = data->lastPlayerPos[0];
    p.cy = data->lastPlayerPos[1];
    p.scale_x = 1.5f;
    p.scale_y = 1.5f;

    DrawLayer::use2D();
      rdpq_mode_push();
      rdpq_mode_begin();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_alphacompare(5);
        rdpq_mode_blender(0);
      rdpq_mode_end();

      rdpq_set_prim_color({0xFF, 0xFF, 0xFF, 0xFF});

      int mapPosX = 320-32;
      int mapPosY = 240-32;

      rdpq_set_scissor(320-64, 240-64, 320, 240);
      rdpq_tex_blit(&data->mapSurf, mapPosX, mapPosY, &p);
      rdpq_set_scissor(0, 0, 320, 240);

      rdpq_mode_pop();
    DrawLayer::useDefault();
  }
}
