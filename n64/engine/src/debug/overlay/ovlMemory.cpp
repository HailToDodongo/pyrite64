/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../overlay.h"
#include "debug/debugDraw.h"
#include "lib/matrixManager.h"
#include "lib/memory.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "vi/swapChain.h"

namespace
{
  #include "ovlColors.h"
  constexpr uint32_t SCREEN_HEIGHT = 240;
  constexpr uint32_t SCREEN_WIDTH = 320;
}

void P64::Debug::Overlay::ovlMemory()
{
  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  //==== Framebuffers ====//
  auto &scene = SceneManager::getCurrent();

  uint16_t posX = 220;
  uint16_t posY = 44;
  uint16_t px{};

  Debug::print(posX, posY, DEBUG_CHAR_SQUARE " Framebuf.");
  posY += 9;

  isMonospace = true;
  for(uint32_t f=0; f<3; ++f) {
    Debug::printf(posX, posY, "FB%ld %08X", f, (uint32_t)P64::VI::SwapChain::getFrameBuffer(f)->buffer);
    posY += 9;
  }
  auto pipeline = scene.getRenderPipeline<RenderPipeline>();
  auto zSurf = pipeline->getCurrDepthSurf();
  Debug::printf(posX, posY, "ZBF %08X", (uint32_t)zSurf->buffer);
  isMonospace = false;

  //==== System Stats ====//
  posX = 26;
  posY = 44;
  heap_stats_t heapStats;
  sys_get_heap_stats(&heapStats);
  auto heapFree = heapStats.total - heapStats.used;

  isMonospace = true;

  Debug::print(posX, posY, DEBUG_CHAR_SQUARE " Heap");

  posY += 9;
  Debug::printf(posX, posY, "Free: %7.2fkb", heapFree / 1024.0);
  posY += 9;

  Debug::setColor(COLOR_MEM_HEAP);
  px = Debug::print(posX, posY, "Used: ");
  Debug::setColor();
  Debug::printf(px, posY, "%7.2fkb (%.1f%%)", heapStats.used / 1024.0, (heapStats.used * 100.0) / heapStats.total);
  posY += 9;

  Debug::setColor(COLOR_MEM_OBJ);
  px = Debug::print(posX, posY, "-Obj: ");
  Debug::setColor();
  Debug::printf(px, posY, "%7.2fkb (%.1f%%)", scene.memObjects / 1024.0, (scene.memObjects * 100.0) / heapStats.used);

  {
    posY += 16;
    auto statMem = P64::Mem::getStaticMemInfo();


    Debug::print(posX, posY, DEBUG_CHAR_SQUARE " Static");
    posY += 9;

    Debug::setColor(COLOR_MEM_TEXT);
    px = Debug::print(posX, posY, " Code: ");
    Debug::setColor();
    Debug::printf(px, posY, "%7.2fkb", statMem.text / 1024.0);
    posY += 9;
    Debug::setColor(COLOR_MEM_DATA);
    px = Debug::print(posX, posY, " Data: ");
    Debug::setColor();
    Debug::printf(px, posY, "%7.2fkb", statMem.data / 1024.0);
    posY += 9;
    Debug::setColor(COLOR_MEM_BSS);
    px = Debug::print(posX, posY, "  BSS: ");
    Debug::setColor();
    Debug::printf(px, posY, "%7.2fkb", statMem.bss / 1024.0);
    posY += 9;
    Debug::setColor(COLOR_MEM_BSS);
    px = Debug::print(posX, posY, " Misc: ");
    Debug::setColor();
    Debug::printf(px, posY, "%7.2fkb", statMem.misc / 1024.0);
    posY += 9;
    Debug::setColor({0xAA, 0xAA, 0xAA, 0xFF});
    Debug::printf(posX, posY, "Total: %7.2fkb", statMem.total / 1024.0);
    Debug::setColor();

    isMonospace = false;
  }

  //==== Matrix Manager ====//

  uint16_t posXStart = 176;
  posX = posXStart;
  posY = 130;

  Debug::print(posX, posY, "Matrices");
  posY += 8;

  rdpq_mode_push();
  rdpq_set_mode_fill({0xFF,0xFF,0xFF, 0xFF});
  bool lastIsUsed = true;

  uint32_t matCount = P64::MatrixManager::getTotalCapacity();
  for(uint32_t i=0; i<matCount; ++i) {
    bool isUsed = P64::MatrixManager::isUsed(i);
    //Debug::print(posX, posY, isUsed ? "+" : ".");
    if(lastIsUsed != isUsed) {
      rdpq_set_fill_color(isUsed ? (color_t){0xFF,0xFF,0xFF, 0xFF} : (color_t){0x33,0x33,0x33, 0xFF});
      lastIsUsed = isUsed;
    }
    rdpq_fill_rectangle(posX, posY, posX+4, posY+4);

    posX += 4;
    if(i % 32 == 31) {
      posX = posXStart;
      posY += 4;
    }
  }
  rdpq_mode_pop();
}

