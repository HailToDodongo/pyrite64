/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../overlay.h"
#include "audio/audioManager.h"
#include "debug/debugDraw.h"
#include "lib/matrixManager.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "vi/swapChain.h"

namespace
{
  #include "ovlColors.h"

  struct TimeEntry {
    const char *label{};
    uint64_t ticks{};
    color_t col{0xFF, 0xFF, 0xFF, 0};
  };

  void printTable(const char* title, uint16_t posX, uint16_t posY, uint16_t txtWidth, const TimeEntry* entries, uint32_t numEntries)
  {
    uint64_t timeSum = 0;
    for(size_t i = 0; i < numEntries; ++i) {
      const auto &entry = entries[i];
      timeSum += entry.ticks;
    }
    timeSum = TICKS_TO_US(timeSum);

    uint16_t posXStart = posX;

    P64::Debug::setColor({0xBB, 0xBB, 0xFF, 0xFF});
    P64::Debug::print(posX, posY, title);
    P64::Debug::setColor();
    posY += 9;

    for(size_t i = 0; i < numEntries; ++i)
    {
      const auto &entry = entries[i];
      auto us = TICKS_TO_US(entry.ticks);
      uint64_t perc = timeSum > 0 ? (us * 100) / timeSum : 0;

      posX = posXStart;
      if(entry.col.a)P64::Debug::setColor(entry.col);
      P64::Debug::print(posX, posY, entry.label);
      if(entry.col.a)P64::Debug::setColor();
      posX = posXStart + txtWidth;

      auto numTxt = std::to_string(us);
      posX += (5 - (numTxt.size())) * 8;

      posX = P64::Debug::print(posX, posY, numTxt.c_str());
      P64::Debug::print(posX, posY, DEBUG_CHAR_US);

      posX += 10;

      if(perc < 10)posX += 8;
      auto percTxt = std::to_string(perc);
      posX = P64::Debug::print(posX, posY, percTxt.c_str());
      P64::Debug::print(posX, posY, "%");

      posY += 9;
    }

    posX = posXStart;
    auto numTxt = std::to_string(timeSum);
    P64::Debug::setColor({0xBB, 0xBB, 0xFF, 0xFF});
    posX = P64::Debug::print(posX, posY, DEBUG_CHAR_SQUARE " Total: ");
    posX = P64::Debug::print(posX, posY, numTxt.c_str());
    P64::Debug::print(posX, posY, DEBUG_CHAR_US);
    P64::Debug::setColor();
  }
}


void P64::Debug::Overlay::ovlCPU()
{
  auto &scene = SceneManager::getCurrent();
  auto &collScene = scene.getCollision();

  setColor();
  uint16_t posY = 42;

  const TimeEntry generalTiming[] = {
    {"Coll Det.", collScene.ticksTotal, COLOR_COLL_DETECT},
    {"Coll Res.", collScene.ticksTotal - collScene.ticksDetect, COLOR_COLL},
    {"Upd. Obj", scene.ticksActorUpdate, COLOR_ACTOR_UPDATE},
    {"Upd. Misc", scene.ticksGlobalUpdate, COLOR_GLOBAL_UPDATE},
    {"Draw Obj", scene.ticksDraw - scene.ticksGlobalDraw, COLOR_SCENE_DRAW},
    {"Draw Misc", scene.ticksGlobalDraw, COLOR_GLOBAL_DRAW},
    {"Audio", P64::AudioManager::ticksUpdate, COLOR_AUDIO},
    {"Debug", ticksSelf},
  };
  printTable(DEBUG_CHAR_SQUARE " General ", 16, posY, 66, generalTiming, sizeof(generalTiming) / sizeof(TimeEntry));

  const TimeEntry collTimingEntries[] = {
    {"Wake", collScene.ticksWakePrep},
    {"World", collScene.ticksWorldUpdate},
    {"Int.Vel", collScene.ticksIntegrateVel},
    {"Det.Body", collScene.ticksDetectBodyPairs},
    {"Det.Mesh", collScene.ticksDetectMeshPairs},
    {"Refresh", collScene.ticksRefreshCallbacks},
    {"PreSolve", collScene.ticksPreSolve},
    {"WarmStart", collScene.ticksWarmStart},
    {"Vel.Solve", collScene.ticksVelocitySolve},
    {"Integrate", collScene.ticksIntegration},
    {"Pos.Solve", collScene.ticksPositionSolve},
    {"Finalize", collScene.ticksFinalize},
  };

  printTable(DEBUG_CHAR_SQUARE " Collision ", 164, posY, 66, collTimingEntries, sizeof(collTimingEntries) / sizeof(TimeEntry));


}
