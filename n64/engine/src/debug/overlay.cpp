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
#include "lib/memory.h"

#include <vector>
#include <string>
#include <filesystem>

namespace P64::SceneManager
{
  extern const char* SCENE_NAMES[];
}

namespace {
  constexpr uint32_t SCREEN_HEIGHT = 240;
  constexpr uint32_t SCREEN_WIDTH = 320;

  constexpr float barWidth = 280.0f;
  constexpr float barHeight = 3.0f;
  constexpr float barRefTimeMs = 1000.0f / 30.0f; // FPS

  constexpr color_t COLOR_BVH{ 0x00, 0xAA, 0x22, 0xFF};
  constexpr color_t COLOR_COLL{0x22,0xFF,0x00, 0xFF};
  constexpr color_t COLOR_ACTOR_UPDATE{0xAA,0,0, 0xFF};
  constexpr color_t COLOR_GLOBAL_UPDATE{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_SCENE_DRAW{0xFF,0x80,0x10, 0xFF};
  constexpr color_t COLOR_GLOBAL_DRAW{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_AUDIO{0x43, 0x52, 0xFF, 0xFF};

  enum class MenuItemType : uint8_t {
    BOOL,
    INT,
    ACTION
  };
  struct MenuItem {
    const char *text{};
    int value{};
    MenuItemType type{};
    std::function<void(MenuItem&)> onChange{};
  };

  struct Menu {
    std::vector<MenuItem> items{};
    uint32_t currIndex;
  };

  constinit Menu menu{};
  constinit Menu menuScenes{};

  constinit T3DMetrics *metrics = nullptr;
  
  uint64_t ticksSelf = 0;

  constexpr float usToWidth(long timeUs) {
    double timeMs = (double)timeUs / 1000.0;
    return (float)(timeMs / (double)barRefTimeMs) * barWidth;
  }

  float frameTimeScale = 2;

  std::vector<std::string> sceneNames{};

  float ticksToOffset(uint32_t ticks) {
    float timeOffX = TICKS_TO_US((uint64_t)ticks) / 1000.0f;
    return timeOffX * frameTimeScale;
  };

  void addBoolItem(Menu &m, const char* name, bool &value) {
    m.items.push_back({name, value, MenuItemType::BOOL, [&value](auto &item) {
      value = item.value;
    }});
  }
  void addActionItem(Menu &m, const char* name, std::function<void(MenuItem&)> action) {
    m.items.push_back({name, 0, MenuItemType::ACTION, action});
  }

  bool showCollMesh = false;
  bool showCollBCS = false;
  bool matrixDebug = false;
  bool showMenuScene = false;
  bool showFrameTime = false;

  bool isVisible = false;
  bool didInit = false;
}

void Debug::Overlay::toggle()
{
  isVisible = !isVisible;
}

namespace fs = std::filesystem;

void Debug::Overlay::init()
{
  sceneNames = {};

  dir_t dir{};
  const char* const BASE_DIR = "rom:/p64";
  int res = dir_findfirst(BASE_DIR, &dir);
  while(res == 0)
  {
    std::string name{dir.d_name};
    if(name[0] == 's' && name.length() == 5) {
      auto id = std::stoi(name.substr(1));
      sceneNames.push_back(name.substr(1) + " - " + P64::SceneManager::SCENE_NAMES[id-1]);
    }
    res = dir_findnext(BASE_DIR, &dir);
  }
}

void Debug::Overlay::draw(P64::Scene &scene, surface_t* surf)
{
  if(!isVisible)
  {
    //Debug::printStart();
    //Debug::printf(20, 16, "%.2f\n", (double)P64::VI::SwapChain::getFPS());
    //Debug::printf(20, 16+8, "%d / %d\n", metrics->trisPostCull, metrics->trisPreCull);
    return;
  }

  if(!metrics)metrics = (T3DMetrics*)malloc_uncached(sizeof(T3DMetrics));
  t3d_metrics_fetch(metrics); // @TODO: remove

  if(!didInit) {
    init();
    didInit = true;
  }

  auto &collScene = scene.getCollision();
  uint64_t newTicksSelf = get_user_ticks();
  MEMORY_BARRIER();

  Debug::draw(surf);

  auto btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

  if(menu.items.empty()) {
    addActionItem(menu, "Scenes", []([[maybe_unused]] auto &item) { showMenuScene = true; });

    addBoolItem(menu, "Coll-Obj", showCollBCS);
    addBoolItem(menu, "Coll-Tri", showCollMesh);
    addBoolItem(menu, "Memory", matrixDebug);
    addBoolItem(menu, "Frames", showFrameTime);

    addActionItem(menuScenes, "< Back >", []([[maybe_unused]] auto &item) {
      showMenuScene = false;
    });

    for(auto &sceneName : sceneNames)
    {
      addActionItem(menuScenes, sceneName.c_str(), [&scene, sceneName]([[maybe_unused]] auto &item) {
        uint32_t sceneId = std::stoi(sceneName.substr(1));
        P64::SceneManager::load(sceneId);
      });
    }
  }

  Menu *currMenu = showMenuScene ? &menuScenes : &menu;

  if(btn.d_up)--currMenu->currIndex;
  if(btn.d_down)++currMenu->currIndex;
  if(currMenu->currIndex > currMenu->items.size() - 1)currMenu->currIndex = 0;

  if(btn.d_left)currMenu->items[currMenu->currIndex].value--;
  if(btn.d_right)currMenu->items[currMenu->currIndex].value++;
  if(btn.d_left || btn.d_right) {
    auto &item = currMenu->items[currMenu->currIndex];
    if(item.type == MenuItemType::BOOL)item.value = (item.value < 0) ? 1 : (item.value % 2);
    item.onChange(item);
  }

  collScene.debugDraw(showCollMesh, showCollBCS);

  float posX = 16;
  float posY = 130;

  if(showFrameTime) {
    constexpr uint32_t fbCount = 3;
    float viBarWidth = 300;

    color_t fbStateCol[6];
    fbStateCol[0] = {0x00, 0x00, 0x00, 0xFF};
    fbStateCol[1] = {0x33, 0xFF, 0x33, 0xFF};
    fbStateCol[2] = {0x22, 0x77, 0x77, 0xFF};
    fbStateCol[3] = {0x22, 0xAA, 0xAA, 0xFF};
    fbStateCol[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    fbStateCol[5] = {0xAA, 0x22, 0xAA, 0xFF};

    rdpq_mode_push();
    rdpq_set_mode_fill({0,0,0, 0xFF});
    rdpq_fill_rectangle(posX, posY-2, posX + viBarWidth, posY + 7 * fbCount+1);
    rdpq_fill_rectangle(posX, posY-10, posX + viBarWidth, posY - 6);

    rdpq_mode_pop();

    Debug::printStart();

    posY = 64;
    Debug::printf(posX + 200, posY-8, "FPS: %.2f", (double)P64::VI::SwapChain::getFPS());

    return;
  }
  Debug::printStart();

  posY = 24;

  heap_stats_t heap_stats;
  sys_get_heap_stats(&heap_stats);

  rdpq_set_prim_color(COLOR_BVH);
  posX = Debug::printf(posX, posY, "Coll:%.2f", (double)TICKS_TO_US(collScene.ticksBVH) / 1000.0) + 4;
  rdpq_set_prim_color(COLOR_COLL);
  posX = Debug::printf(posX, posY, "%.2f", (double)TICKS_TO_US(collScene.ticks - collScene.ticksBVH) / 1000.0) + 8;
  //posX = Debug::printf(posX, posY, "Ray:%d", collScene.raycastCount) + 8;
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

  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  posX = surf->width - 64;
  //posX = Debug::printf(posX, posY, "A:%d/%d", scene.activeActorCount, scene.drawActorCount) + 8;
  // posX = Debug::printf(posX, posY, "T:%d", triCount) + 8;
  Debug::printf(posX-32, posY, "H:%dkb", heap_stats.used);
  Debug::printf(posX, posY+8, "O:%d\n", scene.getObjectCount());

  posX = 24;

  // Menu
  posY = 38;
  for(auto &item : currMenu->items) {
    bool isSel = currMenu->currIndex == (uint32_t)(&item - &currMenu->items[0]);
    switch(item.type) {
      case MenuItemType::INT:
        Debug::printf(posX, posY, "%c %s: %d", isSel ? '>' : ' ', item.text, item.value);
        break;
      case MenuItemType::BOOL:
        Debug::printf(posX, posY, "%c %s: %c", isSel ? '>' : ' ', item.text, item.value ? '1' : '0');
        break;
      case MenuItemType::ACTION:
        Debug::printf(posX, posY, "%c %s", isSel ? '>' : ' ', item.text);
        break;
    }
//    Debug::printf(posX, posY, "%c %s: %d", isSel ? '>' : ' ', item.text, item.value);
    posY += 8;
  }

  // audio channels
  posX = 24;
  posY = SCREEN_HEIGHT - 24;

  posX = Debug::printf(posX, posY, "CH (TODO)");
  /*uint32_t audioMask = scene.getAudio().getActiveChannelMask();
  for(int i=0; i<16; ++i) {
    bool isActive = audioMask & (1 << i);
    posX = Debug::printf(posX, posY, isActive ? "%d" : "-", i);
  }
*/

  // Matrix slots
  if(matrixDebug)
  {
    posX = 100;
    posY = 50;

    for(uint32_t f=0; f<3; ++f) {
      Debug::printf(posX, posY, "Color[%ld]: %p\n", f, P64::VI::SwapChain::getFrameBuffer(f)->buffer);
      posY += 8;
    }

    posY = 90;
    uint32_t matCount = P64::MatrixManager::getTotalCapacity();
    for(uint32_t i=0; i<matCount; ++i) {
      bool isUsed = P64::MatrixManager::isUsed(i);
      Debug::printf(posX, posY, "%c", isUsed ? '+' : '.');
      posX += 6;
      if(i % 32 == 31) {
        posX = 100;
        posY += 8;
      }
    }
  }


  posX = 24;
  posY = 16;

  // Performance graph
  float timeCollBVH = usToWidth(TICKS_TO_US(collScene.ticksBVH));
  float timeColl = usToWidth(TICKS_TO_US(collScene.ticks - collScene.ticksBVH));
  float timeActorUpdate = usToWidth(TICKS_TO_US(scene.ticksActorUpdate));
  float timeGlobalUpdate = usToWidth(TICKS_TO_US(scene.ticksGlobalUpdate));
  float timeSceneDraw = usToWidth(TICKS_TO_US(scene.ticksDraw - scene.ticksGlobalDraw));
  float timeGlobalDraw = usToWidth(TICKS_TO_US(scene.ticksGlobalDraw));
  float timeAudio = usToWidth(TICKS_TO_US(P64::AudioManager::ticksUpdate));
  float timeSelf = usToWidth(TICKS_TO_US(ticksSelf));

  rdpq_set_mode_fill({0,0,0, 0xFF});
  rdpq_fill_rectangle(posX-1, posY-1, posX + (barWidth/2), posY + barHeight+1);
  rdpq_set_mode_fill({0x33,0x33,0x33, 0xFF});
  rdpq_fill_rectangle(posX-1 + (barWidth/2), posY-1, posX + barWidth+1, posY + barHeight+1);

  rdpq_set_fill_color(COLOR_BVH);
  rdpq_fill_rectangle(posX, posY, posX + timeCollBVH, posY + barHeight); posX += timeCollBVH;
  rdpq_set_fill_color(COLOR_COLL);
  rdpq_fill_rectangle(posX, posY, posX + timeColl, posY + barHeight); posX += timeColl;
  rdpq_set_fill_color(COLOR_ACTOR_UPDATE);
  rdpq_fill_rectangle(posX, posY, posX + timeActorUpdate, posY + barHeight); posX += timeActorUpdate;
  rdpq_set_fill_color(COLOR_GLOBAL_UPDATE);
  rdpq_fill_rectangle(posX, posY, posX + timeGlobalUpdate, posY + barHeight); posX += timeGlobalUpdate;
  rdpq_set_fill_color(COLOR_SCENE_DRAW);
  rdpq_fill_rectangle(posX, posY, posX + timeSceneDraw, posY + barHeight); posX += timeSceneDraw;
  rdpq_set_fill_color(COLOR_GLOBAL_DRAW);
  rdpq_fill_rectangle(posX, posY, posX + timeGlobalDraw, posY + barHeight); posX += timeGlobalDraw;
  rdpq_set_fill_color(COLOR_AUDIO);
  rdpq_fill_rectangle(posX, posY, posX + timeAudio, posY + barHeight); posX += timeAudio;
  rdpq_set_fill_color({0xFF,0xFF,0xFF, 0xFF});
  rdpq_fill_rectangle(24 + barWidth - timeSelf, posY, 24 + barWidth, posY + barHeight);

  newTicksSelf = get_user_ticks() - newTicksSelf;
  //if(newTicksSelf < TICKS_FROM_MS(2))
  {
    ticksSelf = newTicksSelf;
  }
}
