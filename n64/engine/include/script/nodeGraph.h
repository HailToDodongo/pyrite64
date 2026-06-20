/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

#include "assets/assetManager.h"

namespace P64
{
  class Object;
}

namespace P64::NodeGraph
{
  typedef void (*GraphFunc)(void* arg);

  // Max number of object references a single graph can declare (see the "Object" node).
  // Object references are resolved to runtime object ids at build time and provided
  // per-instance by the NodeGraph component.
  constexpr int MAX_OBJ_REFS = 8;

  struct GraphDef;
  struct NodeDef;

  class Instance
  {
    private:
      GraphDef* graphDef{};
      coroutine_t *corot{};

    public:
      Object *object{};
      uint32_t args[2]{};
      uint16_t objRefs[MAX_OBJ_REFS]{};
      void* vars{};
      // Seconds the graph has spent running. Advances per active frame, but is frozen
      // while the graph waits/sleeps, so time-driven nodes (waves) stay continuous.
      float time{};
      uint16_t asset{};

      Instance() = default;
      ~Instance();

      void load(uint16_t assetIdx);
      bool update(float deltaTime);
  };

  typedef int(*UserFunc)(uint32_t);

  [[deprecated("Use custom JS nodes instead")]]
  void registerFunction(uint32_t strCRC32, UserFunc fn);

  UserFunc getFunction(uint64_t uuid);
}
