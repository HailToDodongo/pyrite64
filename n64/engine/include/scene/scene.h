/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <vector>
#include "scene/camera.h"
#include "actor/base.h"

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
      std::vector<P64::Camera> cameras{};
      P64::Camera *camMain{nullptr};

      surface_t surfFbColor[3]{};
      T3DMat4FP *objStaticMats{nullptr};
      char* stringTable{nullptr};
      rspq_block_t *dplObjects{nullptr};

      std::vector<Actor::Base*> actors{};
      SceneConf conf{};
      uint16_t id;

      void loadScene();
      void addActor(uint16_t type, Actor::Base::Args *args);

    public:
      explicit Scene(uint16_t sceneId);
      ~Scene();

      void update(float deltaTime);
      void draw(float deltaTime);

      [[nodiscard]] uint16_t getId() const { return id; }
      [[nodiscard]] Camera& getCamera(uint32_t index = 0) { return cameras[index]; }
  };
}