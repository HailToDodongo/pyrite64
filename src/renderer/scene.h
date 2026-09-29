/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <functional>
#include <memory>
#include <SDL3/SDL.h>

#include "pipeline.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace Renderer
{
  class Scene;

  using CbRenderPass = std::function<void(SDL_GPUCommandBuffer*, Scene&)>;
  using CbCopyPass = std::function<void(SDL_GPUCommandBuffer*, SDL_GPUCopyPass*)>;
  using CbPostRender = std::function<void(Scene&)>;

  struct Light
  {
    glm::vec4 color{};
    glm::vec4 pos{};
    glm::vec3 dir{};
    float size{};
    int type{};
  };

  class Scene
  {
    private:
      std::unordered_map<uint32_t, CbRenderPass> renderPasses{};
      std::unordered_map<uint32_t, CbCopyPass> copyPasses{};
      std::unordered_map<uint32_t, CbPostRender> postRenderCallback{};
      std::vector<CbCopyPass> copyPassesOneTime{};

      std::unique_ptr<Shader> shaderN64{};
      std::unique_ptr<Shader> shaderLines{};
      std::unique_ptr<Shader> shaderSprites{};

      // one per z-mode combination, indexed by n64PipelineIdx()
      std::array<std::unique_ptr<Pipeline>, 4> pipelineN64{};
      std::unique_ptr<Pipeline> pipelineLines{};
      std::unique_ptr<Pipeline> pipelineSprites{};

      std::vector<Light> lights{};

    public:
      Scene();
      ~Scene();

      void update();
      void draw();

      void clearLights() { lights.clear(); }
      void addLight(const Light& light) { lights.push_back(light); }
      const std::vector<Light>& getLights() const { return lights; }

      void addRenderPass(uint32_t id, const CbRenderPass& pass) { renderPasses[id] = pass; }
      void removeRenderPass(uint32_t id) { renderPasses.erase(id); }

      void addCopyPass(uint32_t id, const CbCopyPass& pass) { copyPasses[id] = pass; }
      void removeCopyPass(uint32_t id) { copyPasses.erase(id); }

      void addPostRenderCallback(uint32_t id, const CbPostRender& callback) { postRenderCallback[id] = callback; }
      void removePostRenderCallback(uint32_t id) { postRenderCallback.erase(id); }

      void addOneTimeCopyPass(const CbCopyPass& pass) { copyPassesOneTime.push_back(pass); }

      // N64 z-mode packed as bit0 = read, bit1 = write
      static constexpr int n64PipelineIdx(bool depthRead, bool depthWrite) {
        return (depthRead ? 0b01 : 0) | (depthWrite ? 0b10 : 0);
      }

      // N64 pipeline for one z-mode. Depth test/write are fixed-function, so unlike the
      // other render modes they can't be emulated in the shader and need a variant each.
      Pipeline& getN64Pipeline(bool depthRead, bool depthWrite) const {
        return *pipelineN64[n64PipelineIdx(depthRead, depthWrite)];
      }

      Pipeline& getPipeline(const std::string &name) const {
        if (name == "n64") return getN64Pipeline(true, true);
        if (name == "lines") return *pipelineLines;
        if (name == "sprites") return *pipelineSprites;
        throw std::runtime_error("Pipeline not found: " + name);
      }
  };
}
