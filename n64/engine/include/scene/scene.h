/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <vector>

#include "event.h"
#include "lighting.h"
#include "object.h"
#include "collision/scene.h"
#include "scene/camera.h"

namespace P64
{
  struct SceneConf {
    // clears depth/color or not
    constexpr static uint32_t FLAG_CLR_DEPTH = 1 << 0;
    constexpr static uint32_t FLAG_CLR_COLOR = 1 << 1;
    // use RGBA32 over RGBA16 buffer for final output or not
    constexpr static uint32_t FLAG_SCR_32BIT = 1 << 2;

    uint16_t screenWidth{};
    uint16_t screenHeight{};
    uint32_t flags{};
    color_t clearColor{};
    uint32_t objectCount{};
  };

  class Scene
  {
    private:
      std::vector<P64::Camera*> cameras{};
      P64::Camera *camMain{nullptr};

      surface_t surfFbColor[3]{};
      T3DMat4FP *objStaticMats{nullptr};
      char* stringTable{nullptr};
      rspq_block_t *dplObjects{nullptr};

      // @TODO: avoid vector + fragmented alloc
      std::vector<Object*> objects{};

      Coll::Scene collScene{};
      std::vector<Object*> pendingObjDelete{};

      uint32_t eventQueueIdx{0};
      ObjectEventQueue eventQueue[2]{};

      Lighting lighting{};

      SceneConf conf{};
      uint16_t id;

      void loadScene();

    public:
      explicit Scene(uint16_t sceneId, Scene** ref);
      ~Scene();

      void update(float deltaTime);
      void draw(float deltaTime);

      [[nodiscard]] uint16_t getId() const { return id; }
      [[nodiscard]] Camera* getCamera(uint32_t index = 0) { return cameras[index]; }
      [[nodiscard]] Camera& getActiveCamera() { return *camMain; }
      Coll::Scene &getCollision() { return collScene; }

      void sendEvent(uint16_t targetId, uint16_t senderId, uint16_t type, uint32_t value) {
        eventQueue[eventQueueIdx].add(targetId, senderId, type, value);
      }

      void addCamera(Camera *cam) {
        cameras.push_back(cam);
      }

      void removeCamera(Camera *cam) {
        std::erase(cameras, cam);
      }

      void removeObject(Object &obj);

      Object* getObjectById(uint16_t objId) const;

      void setGroupEnabled(uint16_t groupId, bool enabled) const;

      [[nodiscard]] Lighting& getLighting() { return lighting; }
  };
}