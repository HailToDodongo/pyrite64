/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "debug/debugDraw.h"
#include "collision/vec_math.h"
#include <t3d/t3d.h>
#include <cmath>
#include <vector>

#include "font_8x8_IA4.h"

constinit bool P64::Debug::isMonospace = false;

void P64::Debug::printStart() {
  rdpq_mode_begin();
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER1((PRIM,ENV,TEX0,ENV), (0,0,0,TEX0)));
    rdpq_mode_alphacompare(128);
  rdpq_mode_end();

  setColor();
  setBgColor();

  auto surf = surface_make((void*)FONT8x8::DATA, FMT_IA4, FONT8x8::IMG_WIDTH, FONT8x8::IMG_HEIGHT, FONT8x8::IMG_WIDTH/2);
  rdpq_tex_upload(TILE0, &surf, nullptr);
}

extern "C" {
  extern void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3);
}

int P64::Debug::print(uint16_t x, uint16_t y, const char *str) {
  constexpr uint16_t CHAR_PER_ROW = 16;
  constexpr uint16_t CHAR_WIDTH = 8;
  constexpr uint16_t CHAR_HEIGHT = 8;
  constexpr uint16_t MONO_SPACE = 7;

  while(*str)
  {
    uint8_t c = *str - ' ';
    uint8_t charWidth = FONT8x8::WIDTHS[c];
    uint16_t nextX = x + (isMonospace ? MONO_SPACE : charWidth);

    if(c != 0) {
      uint16_t row = c / CHAR_PER_ROW;
      uint16_t col = c % CHAR_PER_ROW;
      if(isMonospace)x += (MONO_SPACE - charWidth) / 2;
      rdpq_texture_rectangle_raw(TILE0, x, y, x+CHAR_WIDTH, y+CHAR_HEIGHT, col*8, row*8, 1, 1);
    }
    x = nextX;
    ++str;
  }
  return x;
}

int P64::Debug::printf(uint16_t x, uint16_t y, const char *fmt, ...) {
  if(x > 320-8)return x;
  char buffer[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, 128, fmt, args);
  va_end(args);
  return Debug::print(x, y, buffer);
}
