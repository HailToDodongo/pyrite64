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

#include "../audio/audioManagerPrivate.h"

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

  constexpr color_t COLOR_COLL_DETECT{ 0x00, 0xAA, 0x22, 0xFF};
  constexpr color_t COLOR_COLL_DETECT_MESH{0x00, 0x88, 0xEE, 0xFF};
  constexpr color_t COLOR_COLL{0x22,0xFF,0x00, 0xFF};
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
  constexpr color_t COLOR_ACTOR_UPDATE{0xAA,0,0, 0xFF};
  constexpr color_t COLOR_GLOBAL_UPDATE{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_SCENE_DRAW{0xFF,0x80,0x10, 0xFF};
  constexpr color_t COLOR_GLOBAL_DRAW{0x33,0x33,0x33, 0xFF};
  constexpr color_t COLOR_AUDIO{0x43, 0x52, 0xFF, 0xFF};

  struct CollTimingEntry {
    const char *label{};
    uint64_t ticks{};
    color_t color{};
  };

  enum class MenuItemType : uint8_t {
    BOOL,
    INT,
    ACTION,
    SUBMENU
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

    void update(joypad_buttons_t btn)
    {
      if(btn.d_up)--currIndex;
      if(btn.d_down)++currIndex;
      if(currIndex > items.size() - 1)currIndex = 0;

      if(btn.d_left)items[currIndex].value--;
      if(btn.d_right)items[currIndex].value++;
      if(btn.d_left || btn.d_right) {
        auto &item = items[currIndex];
        if(item.type == MenuItemType::BOOL)item.value = (item.value < 0) ? 1 : (item.value % 2);
        item.onChange(item);
      }
    }
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

  void addBoolItem(Menu &m, const char* name, bool &value) {
    m.items.push_back({name, value, MenuItemType::BOOL, [&value](auto &item) {
      value = item.value;
    }});
  }
  void addActionItem(Menu &m, const char* name, std::function<void(MenuItem&)> action) {
    m.items.push_back({name, 0, MenuItemType::ACTION, action});
  }
  void addSubMenu(Menu &m, const char* name, std::function<void(MenuItem&)> action) {
    m.items.push_back({name, 0, MenuItemType::SUBMENU, action});
  }

  void toHex2(char out[2], uint32_t in)
  {
    const char *hexDigits = "0123456789ABCDEF";
    out[0] = hexDigits[(in >> 4) & 0xF];
    out[1] = hexDigits[in & 0xF];
  }

  bool showCollMesh = false;
  bool showColliders = false;
  bool matrixDebug = false;
  bool showMenuScene = false;
  bool showFrameTime = false;

  bool isVisible = true;
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
      std::string hex{"00"};
      toHex2(hex.data(), id);
      sceneNames.push_back(hex + " " + P64::SceneManager::SCENE_NAMES[id-1]);
    }
    res = dir_findnext(BASE_DIR, &dir);
  }
}

void Debug::Overlay::draw(P64::Scene &scene, surface_t* surf)
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

  if(!metrics)metrics = (T3DMetrics*)malloc_uncached(sizeof(T3DMetrics));
  t3d_metrics_fetch(metrics); // @TODO: remove

  if(!didInit) {
    init();
    didInit = true;
  }

  auto &collScene = scene.getCollision();
  uint64_t newTicksSelf = get_user_ticks();
  MEMORY_BARRIER();

  auto btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
  Debug::draw(surf);

  if(menu.items.empty()) {
    addSubMenu(menu, "Scenes", []([[maybe_unused]] auto &item) { showMenuScene = true; });

    addBoolItem(menu, "Coll. Obj", showColliders);
    addBoolItem(menu, "Coll. Tri", showCollMesh);
    addBoolItem(menu, "Memory", matrixDebug);
    addBoolItem(menu, "FPS", showFrameTime);

    addActionItem(menuScenes, DEBUG_CHAR_RETURN " Back", []([[maybe_unused]] auto &item) {
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
  currMenu->update(btn);

  collScene.debugDraw(showCollMesh, showColliders);

  Debug::printStart();

  heap_stats_t heap_stats;
  sys_get_heap_stats(&heap_stats);
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
  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  //posX = Debug::printf(posX, posY, "A:%d/%d", scene.activeActorCount, scene.drawActorCount) + 8;
  // posX = Debug::printf(posX, posY, "T:%d", triCount) + 8;
  //Debug::printf(posX-32, posY, "H:%dkb", heap_stats.used);
  //Debug::printf(posX, posY+8, "O:%d\n", scene.getObjectCount());

  int posX = 24;
  int posY = 30;

  // Menu
  for(auto &item : currMenu->items) {
    bool isSel = currMenu->currIndex == (uint32_t)(&item - &currMenu->items[0]);
    if(isSel) {
      setBgColor({0, 0, 0x55, 0xFF});
      Debug::print(posX, posY, DEBUG_CHAR_ARROW);
      setBgColor();
    }

    int px = posX + 10;

    switch(item.type) {
      case MenuItemType::INT:
        Debug::printf(px, posY, "%s: %d", item.text, item.value);
        break;
      case MenuItemType::BOOL:
      {
        if(item.value)setColor({0x22, 0xAA, 0x22, 0xFF});
        px = Debug::print(px, posY, item.value ? (DEBUG_CHAR_CHECK_ON " ") : (DEBUG_CHAR_CHECK_0FF " "));
        if(item.value)setColor();
        Debug::print(px, posY, item.text);
      }
      break;
      case MenuItemType::ACTION:
        px = Debug::print(px, posY, DEBUG_CHAR_FUNC " ");
        Debug::print(px, posY, item.text);
        break;
      case MenuItemType::SUBMENU:
        px = Debug::print(px, posY, DEBUG_CHAR_DIR " ");
        Debug::print(px, posY, item.text);
        break;
    }

//    Debug::printf(posX, posY, "%c %s: %d", isSel ? '>' : ' ', item.text, item.value);
    posY += 8;
  }

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

  posX = 140;
  posY = 38;

  for(size_t i = 0; i < std::size(collTimingEntries); ++i) {
    const CollTimingEntry &entry = collTimingEntries[i];
    if((i % 2) == 0 && i != 0) {
      posY += 8;
    }

    float colX = (i % 2) == 0 ? 140.0f : 228.0f;
    rdpq_set_prim_color(entry.color);
    Debug::printf(colX, posY, "%s: %lld" DEBUG_CHAR_US, entry.label, TICKS_TO_US(entry.ticks));
  }

  rdpq_set_prim_color({0xFF,0xFF,0xFF, 0xFF});

  // audio channels
  posX = 24;
  posY = SCREEN_HEIGHT - 24;

  isMonospace = true;
  posX = Debug::printf(posX, posY, "Audio ");
  {
    auto audioMetrics = P64::AudioManager::getMetrics();
    char strMask[33] = {};
    strMask[32] = '\0';
    for(uint32_t i=0; i<32; ++i) {
      bool isPlaying = audioMetrics.maskPlaying & (1 << i);
      bool isUsed    = audioMetrics.maskAlloc & (1 << i);

      if(isPlaying && isUsed)strMask[i] = DEBUG_CHAR_SQUARE[0];
      else if(isUsed)strMask[i] = '-';
      else if(isPlaying)strMask[i] = '?';
      else strMask[i] = '.';
    }
    Debug::print(posX, posY, strMask);
  }
  isMonospace = false;

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

  // Top bar for CPU time
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

  //debugf("Self: %fms\n", (double)TICKS_TO_US(ticksSelf) / 1000.0);

  // Bottom bar for RAM

}
