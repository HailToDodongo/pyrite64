/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "fonts.h"
#include <libdragon.h>
#include <assets/assetManager.h>
#include "../../p64/assetTable.h"

namespace
{
  sprite_t *texNumbers;
}

void P64::User::Fonts::init()
{
  texNumbers = (sprite_t*)AssetManager::getByIndex("ui/numbers.ia8.sprite"_asset);
}

void P64::User::Fonts::destroy()
{
}

void P64::User::Fonts::useNumber()
{
  rdpq_sprite_upload(TILE0, texNumbers, nullptr);
  rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
}

void P64::User::Fonts::printNumber(int x, int y, uint32_t num)
{
  constexpr float scale = 0.9f;
  constexpr float sX = 14 * scale;
  constexpr float sY = 16 * scale;
  constexpr float spacing = 10 * scale;

  if(num > 9)x += spacing;
  if(num > 99)x += spacing;
  if(num > 999)x += spacing;

  do {
    int s = (num % 10) * 14;
    rdpq_texture_rectangle_scaled(TILE0, x, y, x+sX, y+sY, s, 0, s+14, 16);
    x -= spacing;
    num /= 10;
  } while(num > 0);
}
