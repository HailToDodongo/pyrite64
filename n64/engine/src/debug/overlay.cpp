/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "overlay.h"

#include <cstring>

#include "debug/debugDraw.h"
#include "debug/debugMenu.h"
#include "scene/scene.h"
#include "vi/swapChain.h"
#include "audio/audioManager.h"

#include <vector>
#include <string>
#include <filesystem>

#include "../../include/debug/menu.h"
#include "../audio/audioManagerPrivate.h"
#include "lib/memory.h"

namespace P64::SceneManager
{
  extern const char* SCENE_NAMES[];
}

constinit uint64_t P64::Debug::Overlay::ticksSelf = 0;
constinit bool P64::Debug::Overlay::useCpuAvg = true;

namespace {
  #include "overlay/ovlColors.h"

  constexpr float barWidth = 280.0f;
  constexpr float barHeight = 3.0f;
  constexpr float barRefTimeMs = 1000.0f / 30.0f; // FPS
  constexpr float barRefBytes = 1024 * 1024 * 8; // 8mb

  constinit P64::Debug::Menu menu{};
  constinit P64::Debug::Menu menuScenes{};
  constinit P64::Debug::Menu menuColl{};
  constinit P64::Debug::Menu menuAudio{};
  constinit P64::Debug::Menu menuMemory{};
  constinit P64::Debug::Menu menuCPU{};

  constexpr float usToWidth(long timeUs) {
    double timeMs = (double)timeUs / 1000.0;
    return (float)(timeMs * (1.0 / (double)barRefTimeMs)) * barWidth;
  }

  constexpr float bytesToWidth(size_t bytes) {
    double ratio = (double)bytes * (1.0 / (double)barRefBytes);
    return (float)ratio * barWidth;
  }

  bool showCollMesh = false;
  bool showColliders = false;
  bool showFrameTime = false;
  bool showBarCPU = true;
  bool showBarRAM = true;

  bool isVisible = false;
}

void P64::Debug::Overlay::toggle()
{
  isVisible = !isVisible;
}

namespace fs = std::filesystem;

void P64::Debug::Overlay::init()
{
  auto &scene = P64::SceneManager::getCurrent();

  for(auto &item : menu.items) {
    if(item.type == MenuItemType::SUBMENU) {
      if(item.getMenu()) {
        if(menu.activSubMenu == item.getMenu()) {
          menu.activSubMenu = nullptr;
        }
        delete item.getMenu();
      }
    }
  }

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
      .add("Bar CPU", showBarCPU)
      .add("Bar RAM", showBarRAM)
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
  menuCPU.add("Average", useCpuAvg);

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

P64::Debug::Menu& P64::Debug::Overlay::addCustomMenu(const char* name)
{
  auto m = new Menu();
  menu.add(name, *m);
  menu.items.back().value = m;
  return *m;
}

void P64::Debug::Overlay::removeCustomMenu(const char* name)
{
  for(auto it = menu.items.begin(); it != menu.items.end(); ++it) {
    if(it->type == MenuItemType::SUBMENU && std::strcmp(it->text, name) == 0) {
      if(it->getMenu())delete it->getMenu();
      menu.items.erase(it);
      break;
    }
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

  heap_stats_t heapStats;
  sys_get_heap_stats(&heapStats);

  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});
  menu.draw();

  // Top bar for CPU time
  if(showBarCPU)
  {
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

    // Measure self-time
    float timeSelf = usToWidth(TICKS_TO_US(ticksSelf));
    rdpq_set_fill_color({0xFF,0xFF,0xFF, 0xFF});
    rdpq_fill_rectangle(24 + barWidth - timeSelf, posY, 24 + barWidth, posY + barHeight);
  }

  // RAM graph
  if(showBarRAM)
  {
    auto memInfo = P64::Mem::getStaticMemInfo();
    float memUsed = (float)(memInfo.text + memInfo.data + memInfo.bss) / (float)memInfo.total;

    uint16_t posX = 24;
    uint16_t posY = 240-24;

    rdpq_set_mode_fill({0,0,0, 0xFF});
    // make in alternating color at 1Mb marks
    posX -= 1;
    for(int i = 0; i < 8; i++) {
      rdpq_set_fill_color((i % 2 == 0) ? color_t{0x33,0x33,0x33, 0xFF} : color_t{0,0,0, 0xFF});
      rdpq_fill_rectangle(posX + (barWidth * ((float)i / 8.0f)), posY-1, posX + (barWidth * ((float)(i+1) / 8.0f)), posY + barHeight+1);
    }
    posX += 1;

    auto addBarSection = [&](uint64_t bytes, color_t color) {
      float time = bytesToWidth(bytes);
      if(time > 0.0f) {
        rdpq_set_fill_color(color);
        rdpq_fill_rectangle(posX, posY, posX + time, posY + barHeight);
        posX += time;
      }
    };

    addBarSection(memInfo.text, COLOR_MEM_TEXT);
    addBarSection(memInfo.data, COLOR_MEM_DATA);
    addBarSection(memInfo.bss + memInfo.misc, COLOR_MEM_BSS);
    addBarSection(scene.memObjects, COLOR_MEM_OBJ);
    addBarSection(heapStats.used - scene.memObjects, COLOR_MEM_HEAP);
  }

  ticksSelf = get_user_ticks() - newTicksSelf;
  //debugf("Self: %fms\n", (double)TICKS_TO_US(ticksSelf) / 1000.0);
}
