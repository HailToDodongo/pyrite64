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
#include "lib/types.h"
#include "renderer/drawLayer.h"
#include "renderer/pipeline.h"
#include "scene/camera.h"

namespace P64
{
  class RenderPipelineBigTex;
}

namespace P64
{
  class RenderPipelineHDRBloom;
}

namespace P64
{
  class RenderPipeline;

  struct SceneConf {

    enum class Pipeline : uint8_t
    {
      DEFAULT,
      HDR_BLOOM,
      BIG_TEX_256
    };

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

    Pipeline pipeline{};
    uint8_t padding[3]{};

    DrawLayer::Setup layerSetup{};
  };

  class Scene
  {
    private:
      std::vector<Camera*> cameras{};
      Camera *camMain{nullptr};

      RenderPipeline *renderPipeline{nullptr};

      // @TODO: avoid vector + fragmented alloc
      std::vector<Object*> objects{};

      Coll::Scene collScene{};
      std::vector<Object*> pendingObjDelete{};

      uint32_t eventQueueIdx{0};
      ObjectEventQueue eventQueue[2]{};

      Lighting lighting{};

      SceneConf conf{};
      uint16_t id;

      void loadSceneConfig();
      void loadScene();

    public:
      explicit Scene(uint16_t sceneId, Scene** ref);
      ~Scene();

      CLASS_NO_COPY_MOVE(Scene);

      void update(float deltaTime);
      void draw(float deltaTime);

      [[nodiscard]] const SceneConf& getConf() const { return conf; }
      [[nodiscard]] uint16_t getId() const { return id; }
      [[nodiscard]] Camera* getCamera(uint32_t index = 0) { return cameras[index]; }
      [[nodiscard]] Camera& getActiveCamera() { return *camMain; }
      Coll::Scene &getCollision() { return collScene; }

      void onObjectCollision(const Coll::CollEvent &event);

      void sendEvent(uint16_t targetId, uint16_t senderId, uint16_t type, uint32_t value) {
        eventQueue[eventQueueIdx].add(targetId, senderId, type, value);
      }

      void addCamera(Camera *cam) {
        cameras.push_back(cam);
      }

      void removeCamera(Camera *cam) {
        std::erase(cameras, cam);
      }

      /**
       * Returns current (typed) rendering pipeline.
       * If the expected type is not the correct one, returns nullptr.
       * This function can be used to safely access pipeline-specific features.
       *
       * @tparam T Type of the expected rendering pipeline
       * @return Pointer to the rendering pipeline of type T, or nullptr if the type does not match
       */
      template<typename T>
      T* getRenderPipeline() {
        if constexpr (std::is_same_v<T, RenderPipelineDefault>) {
          if(conf.pipeline != SceneConf::Pipeline::DEFAULT)return nullptr;
        }
        if constexpr (std::is_same_v<T, RenderPipelineHDRBloom>) {
          if(conf.pipeline != SceneConf::Pipeline::HDR_BLOOM)return nullptr;
        }
        if constexpr (std::is_same_v<T, RenderPipelineBigTex>) {
          if(conf.pipeline != SceneConf::Pipeline::BIG_TEX_256)return nullptr;
        }
        return static_cast<T*>(renderPipeline);
      }

      void removeObject(Object &obj);

      Object* getObjectById(uint16_t objId) const;

      void setGroupEnabled(uint16_t groupId, bool enabled) const;

      [[nodiscard]] Lighting& getLighting() { return lighting; }
  };
}
