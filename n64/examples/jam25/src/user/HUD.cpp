#include <vi/swapChain.h>

#include "globals.h"
#include "script/userScript.h"
#include "systems/context.h"
#include "systems/fonts.h"

namespace
{
  constexpr int SCREEN_EDGE = 20;
  constexpr float COIN_UPDATE_TIME = 0.1f;
}

namespace P64::Script::C7D6052B4FD1AF1C
{
  P64_DATA(
    [[P64::Name("Icon Coin")]]
    AssetRef<sprite_t> iconCoin;

    [[P64::Name("Icon Health")]]
    AssetRef<sprite_t> iconHealth;

    rspq_block_t *dplCoins;
    uint32_t displayCoins;
    float timer;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      rspq_block_free(data->dplCoins);
      return;
    }

    const auto &conf = SceneManager::getCurrent().getConf();
    int posX = SCREEN_EDGE;
    int posY = conf.screenHeight - SCREEN_EDGE - data->iconCoin.get()->height;

    rspq_block_begin();
      rdpq_mode_begin();
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_alphacompare(2);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
      rdpq_mode_end();

      rdpq_blitparms_t p{};
      //p.scale_y = p.scale_x = 0.75f;
      rdpq_sprite_blit(data->iconCoin.get(), posX, posY, &p);

      rdpq_mode_blender(0);
      rdpq_mode_combiner(RDPQ_COMBINER1((1,TEX0,PRIM,TEX0), (TEX0,0,PRIM,0)));
      rdpq_set_prim_color({0xDD, 0x55, 0x55, 0xFF});
      rdpq_sprite_upload(TILE0, data->iconHealth.get(), nullptr);

    data->dplCoins = rspq_block_end();
    data->displayCoins = User::ctx.coins;
    data->timer = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    // collected coins, show directly:
    if(data->displayCoins < User::ctx.coins) {
      data->displayCoins = User::ctx.coins;
      return;
    }
    // loosing coins, decrease slowly:
    if(data->displayCoins > User::ctx.coins) {
      data->timer += deltaTime;
      if(data->timer > COIN_UPDATE_TIME) {
        data->timer = 0;
        data->displayCoins -= 1;
      }
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    int baseX = SCREEN_EDGE;
    int baseY = SCREEN_EDGE;
    constexpr int HEALTH_SIZE = 12;

    DrawLayer::use2D();
      // Coins
      rspq_block_run(data->dplCoins);

      // Health
      for(uint32_t i=0; i<User::ctx.healthTotal; i+=4) {
        uint32_t uvOffset = 0;
        if(i < (uint32_t)User::ctx.health) {
          uvOffset = Math::min((uint32_t)4, User::ctx.health - i);
        }

        rdpq_texture_rectangle(TILE0, baseX, baseY, baseX+HEALTH_SIZE, baseY+HEALTH_SIZE, uvOffset * HEALTH_SIZE, 0);
        baseX += (HEALTH_SIZE+1);
      }

      // Coin Text
      //rdpq_text_printf(nullptr, User::FONT_TITLE, 48, 240-SCREEN_EDGE-8, "%ld", User::ctx.coins);
      rdpq_set_prim_color({0xFF, 0xFF, 0xFF, 0xFF});
      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      User::Fonts::useNumber();
      //User::Fonts::printNumber(320 - SCREEN_EDGE - 32, baseY, data->displayCoins);
      User::Fonts::printNumber(SCREEN_EDGE+20, 240-baseY-17, data->displayCoins);

    DrawLayer::useDefault();
  }
}
