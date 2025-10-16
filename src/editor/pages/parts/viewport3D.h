/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>

#include "../../../renderer/camera.h"
#include "../../../renderer/vertBuffer.h"
#include "../../../renderer/framebuffer.h"
#include "../../../renderer/mesh.h"
#include "../../../renderer/object.h"

namespace Editor
{
  class Viewport3D
  {
    private:
      Renderer::UniformGlobal uniGlobal{};
      Renderer::Framebuffer fb{};
      Renderer::Camera camera{};
      uint32_t passId{};

      bool isMouseHover{false};
      bool isMouseDown{false};

      glm::vec2 mousePos{};
      glm::vec2 mousePosStart{};

      // TEST:
      std::shared_ptr<Renderer::Mesh> mesh{};

      Renderer::Object obj{};
      Renderer::Object obj2{};

      void onRenderPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUGraphicsPipeline* pipeline);
      void onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass);

    public:
      Viewport3D();
      ~Viewport3D();

      void draw();
  };
}
