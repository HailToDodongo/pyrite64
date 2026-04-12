/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../overlay.h"
#include "debug/debugDraw.h"
#include "lib/matrixManager.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "vi/swapChain.h"

namespace
{
  constexpr uint32_t SCREEN_HEIGHT = 240;
  constexpr uint32_t SCREEN_WIDTH = 320;

  constexpr color_t COLOR_COLL_DETECT{ 0x00, 0xAA, 0x22, 0xFF};
  constexpr color_t COLOR_COLL_WAKE{0x00, 0x88, 0xCC, 0xFF};
  constexpr color_t COLOR_COLL_WORLD{0x00, 0xCC, 0x88, 0xFF};
  constexpr color_t COLOR_COLL_INTEGRATE_VEL{0x66, 0xCC, 0x00, 0xFF};
  constexpr color_t COLOR_COLL_REFRESH{0xCC, 0xCC, 0x00, 0xFF};
  constexpr color_t COLOR_COLL_PRESOLVE{0xFF, 0xA0, 0x00, 0xFF};
  constexpr color_t COLOR_COLL_WARM{0xFF, 0x66, 0x00, 0xFF};
  constexpr color_t COLOR_COLL_VEL_SOLVE{0xFF, 0x22, 0x22, 0xFF};
  constexpr color_t COLOR_COLL_INTEGRATE_POS{0xCC, 0x33, 0x88, 0xFF};
  constexpr color_t COLOR_COLL_POS_SOLVE{0x88, 0x44, 0xCC, 0xFF};
  constexpr color_t COLOR_COLL_FINALIZE{0x66, 0x66, 0x66, 0xFF};
  constexpr color_t COLOR_COLL_DETECT_MESH{0x00, 0x88, 0xEE, 0xFF};

  struct CollTimingEntry {
    const char *label{};
    uint64_t ticks{};
    color_t color{};
  };
}


void P64::Debug::Overlay::ovlCPU()
{
  auto &scene = SceneManager::getCurrent();
  auto &collScene = scene.getCollision();

  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  const CollTimingEntry collTimingEntries[] = {
    {"Wake", collScene.ticksWakePrep, COLOR_COLL_WAKE},
    {"World", collScene.ticksWorldUpdate, COLOR_COLL_WORLD},
    {"IntV", collScene.ticksIntegrateVel, COLOR_COLL_INTEGRATE_VEL},
    {"DetB", collScene.ticksDetectBodyPairs, COLOR_COLL_DETECT},
    {"DetM", collScene.ticksDetectMeshPairs, COLOR_COLL_DETECT_MESH},
    {"Refresh", collScene.ticksRefreshCallbacks, COLOR_COLL_REFRESH},
    {"Pre", collScene.ticksPreSolve, COLOR_COLL_PRESOLVE},
    {"Warm", collScene.ticksWarmStart, COLOR_COLL_WARM},
    {"Vel", collScene.ticksVelocitySolve, COLOR_COLL_VEL_SOLVE},
    {"IntP", collScene.ticksIntegration, COLOR_COLL_INTEGRATE_POS},
    {"Pos", collScene.ticksPositionSolve, COLOR_COLL_POS_SOLVE},
    {"Final", collScene.ticksFinalize, COLOR_COLL_FINALIZE},
  };

  uint16_t posX = 140;
  uint16_t posY = 38;

  for(size_t i = 0; i < std::size(collTimingEntries); ++i) {
    const CollTimingEntry &entry = collTimingEntries[i];
    if((i % 2) == 0 && i != 0) {
      posY += 8;
    }

    float colX = (i % 2) == 0 ? 140.0f : 228.0f;
    rdpq_set_prim_color(entry.color);
    Debug::printf(colX, posY, "%s: %lld" DEBUG_CHAR_US, entry.label, TICKS_TO_US(entry.ticks));
  }

  /*
  rdpq_set_prim_color(COLOR_COLL_DETECT);
  posX = Debug::printf(posX, posY, "Coll:%.2f", (double)TICKS_TO_US(collScene.ticksDetect) / 1000.0) + 4;
  rdpq_set_prim_color(COLOR_COLL);
  posX = Debug::printf(posX, posY, "%.2f", (double)TICKS_TO_US(collScene.ticksTotal) / 1000.0) + 8;
  rdpq_set_prim_color(COLOR_ACTOR_UPDATE);
  Debug::printf(posX, posY, "%.2f", (double)TICKS_TO_US(scene.ticksActorUpdate) / 1000.0);
    rdpq_set_prim_color(COLOR_GLOBAL_UPDATE);
    posX = Debug::printf(posX, posY + 8, "%.2f", (double)TICKS_TO_US(scene.ticksGlobalUpdate) / 1000.0) + 8;
  rdpq_set_prim_color(COLOR_SCENE_DRAW);
  Debug::printf(posX, posY, "%.2f", (double)TICKS_TO_US(scene.ticksDraw - scene.ticksGlobalDraw) / 1000.0);
    rdpq_set_prim_color(COLOR_GLOBAL_DRAW);
    posX = Debug::printf(posX, posY+8, "%.2f", (double)TICKS_TO_US(scene.ticksGlobalDraw) / 1000.0)+ 8;
  rdpq_set_prim_color(COLOR_AUDIO);
  posX = Debug::printf(posX, posY, "%.2f", (double)TICKS_TO_US(P64::AudioManager::ticksUpdate) / 1000.0) + 8;
*/


}
