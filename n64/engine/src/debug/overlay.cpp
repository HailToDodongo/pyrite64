/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "overlay.h"

#include "debug/debugDraw.h"
#include "scene/scene.h"
#include "vi/swapChain.h"
#include "audio/audioManager.h"
#include "lib/matrixManager.h"

#include <vector>
#include <string>
#include <filesystem>

#include "menu.h"
#include "../audio/audioManagerPrivate.h"

namespace P64::SceneManager
{
  extern const char* SCENE_NAMES[];
}

namespace {
  constexpr float barWidth = 280.0f;
  constexpr float barHeight = 3.0f;
  constexpr float barRefTimeMs = 1000.0f / 30.0f; // FPS

  constexpr color_t COLOR_COLL_DETECT{ 0x00, 0xAA, 0x22, 0xFF};
  constexpr color_t COLOR_COLL{0x22,0xFF,0x00, 0xFF};
  constexpr color_t COLOR_ACTOR_UPDATE{0xAA,0,0, 0xFF};
  constexpr color_t COLOR_GLOBAL_UPDATE{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_SCENE_DRAW{0xFF,0x80,0x10, 0xFF};
  constexpr color_t COLOR_GLOBAL_DRAW{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_AUDIO{0x43, 0x52, 0xFF, 0xFF};

  constinit P64::Debug::Menu menu{};
  constinit P64::Debug::Menu menuScenes{};
  constinit P64::Debug::Menu menuColl{};
  constinit P64::Debug::Menu menuAudio{};
  constinit P64::Debug::Menu menuMemory{};
  constinit P64::Debug::Menu menuCPU{};

  uint64_t ticksSelf = 0;

  constexpr float usToWidth(long timeUs) {
    double timeMs = (double)timeUs / 1000.0;
    return (float)(timeMs / (double)barRefTimeMs) * barWidth;
  }

  bool showCollMesh = false;
  bool showColliders = false;
  bool showFrameTime = true;
  bool isVisible = true;
}

void P64::Debug::Overlay::toggle()
{
  isVisible = !isVisible;
}

namespace fs = std::filesystem;

void P64::Debug::Overlay::init()
{
  auto &scene = P64::SceneManager::getCurrent();

  menu.items.clear();
  menuColl.items.clear();
  menuScenes.items.clear();
  menuAudio.items.clear();
  menuMemory.items.clear();
  menuCPU.items.clear();

  menu.add("Scenes", menuScenes)
      .add("CPU", menuCPU)
      .add("Collision", menuColl)
      .add("Audio", menuAudio)
      .add("Memory", menuMemory)
      .add("FPS", showFrameTime)
    ;

  menuColl
    .add("Show Obj.", showColliders)
    .add("Show Mesh", showCollMesh)
    .add("Ticks", scene.getConf().physicsTickRate, 1, 120, 1)
    .add("Iter. Pos", scene.getConf().positionSolverIterations, 1, 20, 1)
    .add("Iter. Vel", scene.getConf().velocitySolverIterations, 1, 20, 1)
    .add("Interp.", scene.getConf().interpolatePhysicsTransforms)
  ;

  menuAudio.onDraw = ovlAudio;
  menuAudio.add("Freq.", scene.getConf().audioFreq, 8000, 48000, 0);
  menuAudio.add("Volume", P64::AudioManager::masterVol, 0.0f, 1.0f, 0.05f);

  menuMemory.onDraw = ovlMemory;
  menuCPU.onDraw = ovlCPU;

  dir_t dir{};
  const char* const BASE_DIR = "rom:/p64";
  int res = dir_findfirst(BASE_DIR, &dir);
  while(res == 0)
  {
    std::string name{dir.d_name};
    if(name[0] == 's' && name.length() == 5) {
      auto id = std::stoi(name.substr(1));
      menuScenes.add(P64::SceneManager::SCENE_NAMES[id-1], [id]([[maybe_unused]] auto &item) {
        SceneManager::load(id);
      });
    }
    res = dir_findnext(BASE_DIR, &dir);
  }
}

void P64::Debug::Overlay::draw(surface_t* surf)
{
  if(showFrameTime) {
    Debug::printStart();
    isMonospace = true;
    Debug::printf(24, 22, "%.2f", (double)P64::VI::SwapChain::getFPS());
    isMonospace = false;
  }

  if(!isVisible) {
    return;
  }

  auto &scene = SceneManager::getCurrent();
  auto &collScene = scene.getCollision();
  uint64_t newTicksSelf = get_user_ticks();
  MEMORY_BARRIER();

  Debug::draw(surf);
  menu.update();

  collScene.debugDraw(showCollMesh, showColliders);

  Debug::printStart();

  heap_stats_t heap_stats;
  sys_get_heap_stats(&heap_stats);

  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  //posX = Debug::printf(posX, posY, "A:%d/%d", scene.activeActorCount, scene.drawActorCount) + 8;
  // posX = Debug::printf(posX, posY, "T:%d", triCount) + 8;
  //Debug::printf(posX-32, posY, "H:%dkb", heap_stats.used);
  //Debug::printf(posX, posY+8, "O:%d\n", scene.getObjectCount());

  menu.draw();

  // Top bar for CPU time
  uint16_t posX = 24;
  uint16_t posY = 16;

  rdpq_set_mode_fill({0,0,0, 0xFF});
  rdpq_fill_rectangle(posX-1, posY-1, posX + (barWidth/2), posY + barHeight+1);
  rdpq_set_fill_color({0x33,0x33,0x33, 0xFF});
  rdpq_fill_rectangle(posX-1 + (barWidth/2), posY-1, posX + barWidth+1, posY + barHeight+1);

  auto addBarSection = [&](uint64_t ticks, color_t color) {
    float time = usToWidth(TICKS_TO_US(ticks));
    if(time > 0.0f) {
      rdpq_set_fill_color(color);
      rdpq_fill_rectangle(posX, posY, posX + time, posY + barHeight);
      posX += time;
    }
  };

  addBarSection(collScene.ticksDetect, COLOR_COLL_DETECT);
  addBarSection(collScene.ticksTotal - collScene.ticksDetect, COLOR_COLL);
  addBarSection(scene.ticksActorUpdate, COLOR_ACTOR_UPDATE);
  addBarSection(scene.ticksGlobalUpdate, COLOR_GLOBAL_UPDATE);
  addBarSection(scene.ticksDraw - scene.ticksGlobalDraw, COLOR_SCENE_DRAW);
  addBarSection(scene.ticksGlobalDraw, COLOR_GLOBAL_DRAW);
  addBarSection(P64::AudioManager::ticksUpdate, COLOR_AUDIO);

  float timeSelf = usToWidth(TICKS_TO_US(ticksSelf));
  rdpq_set_fill_color({0xFF,0xFF,0xFF, 0xFF});
  rdpq_fill_rectangle(24 + barWidth - timeSelf, posY, 24 + barWidth, posY + barHeight);
  ticksSelf = get_user_ticks() - newTicksSelf;

  debugf("Self: %fms\n", (double)TICKS_TO_US(ticksSelf) / 1000.0);
}
