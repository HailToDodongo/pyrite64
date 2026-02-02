#include <debug/debugDraw.h>
#include "script/userScript.h"
#include "systems/sprites.h"

namespace {
  constexpr uint32_t TYPE_COIN      = 0;
  constexpr uint32_t TYPE_COIN_BLUE = 1;
  constexpr uint32_t TYPE_HURT      = 2;
  constexpr uint32_t TYPE_SPARK     = 3;

  constexpr float PARTICLE_TIME = 1.2f;
  constexpr float PARTICLE_TIME_SPARK = 0.45f;

  color_t sparkGradient(float s) {
    s = fminf(1.0f, s + 0.1f);
    s = P64::Math::easeOutCubic(s);
    // smooth gradient from blue to yellow to red
    color_t color;
    if(s < 0.5f) {
      color.r = 0x60 + (uint8_t)(s * 0x9F);
      color.g = 0xA0 + (uint8_t)(s * 0x5F);
      color.b = 0xFF;
    } else {
      color.r = 0xFF;
      color.g = 0xFF - (uint8_t)((s - 0.5f) * 0x5F);
      color.b = 0xFF - (uint8_t)((s - 0.5f) * 0x5F);
    }
    color.a = 0xFF;
    return color;
  }
}

namespace P64::Script::C97E100FE63D9085
{
  P64_DATA(
    float timer;
    uint32_t type;
    std::array<fm_vec3_t, 5> dirs;
    color_t color;
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      return;
    }

    data->timer = 0;
    data->type = TYPE_COIN;

    for(auto &dir: data->dirs) {
      dir = Math::randDir3D();
      if(data->type != TYPE_SPARK) {
        dir.y += 0.75f;
      }
    }

    switch(data->type) {
      case TYPE_COIN:      data->color = User::Sprites::coin->getColor(); break;
      case TYPE_COIN_BLUE: data->color = {0x60,0xA0,0xFF,0xFF}; break;
      case TYPE_HURT:      data->color = {0xFF,0x40,0x40,0xFF}; break;
      default: data->color = {0xFF,0xFF,0xFF,0xFF}; break;
    }

    data->timer = 0.0f;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    float maxTime = data->type == TYPE_SPARK ? PARTICLE_TIME_SPARK : PARTICLE_TIME;
    if(data->timer >= maxTime)return obj.remove();
    //if(!checkCulling(130.0f))return;

    data->timer += deltaTime;
    uint16_t seed = (uint32_t)(void*)(data) >> 8;

    if(data->type == TYPE_SPARK) {
      float partScale = (1.0f - (data->timer / maxTime)) * 0.4f;
      float dirScale = (data->timer+0.08f) * 49.0f;
      data->color = sparkGradient(data->timer / maxTime);

      int parts = 4;
      for(auto &dir: data->dirs) {
        float localScale = partScale;
        auto partPos = obj.pos * 1 + dir * dirScale;
        for(int i=0; i<parts; ++i) {
          // scene.getPTSpark().add(partPos, 0, data->color, localScale);
          partPos -= dir * 1.2f;
          localScale *= 0.82f;
        }
      }
    } else {
      float partScale = (1.0f - (data->timer / PARTICLE_TIME)) * 0.8f;
      float dirScale = (data->timer+0.15f) * 24.0f;
      for(auto &dir: data->dirs) {
        auto partPos = obj.pos + dir * dirScale;
        //auto &pt = data->type == TYPE_HURT ? scene.getPTHeart() : scene.getPTFragment();

        //Debug::drawLine(obj.pos, partPos, {0xFF, 0x00, 0x00, 0xFF});

        User::Sprites::coinPart->add(partPos, seed, data->color, partScale);

        seed += 0x1234;
        dir.y -= 0.75f * deltaTime;
      }
    }

  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
  }
}
