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
#include "../../../utils/container.h"

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
      Utils::RequestVal<uint32_t> pickedObjID{};
      bool pickAdditive{false};
      bool selectionPending{false};
      bool selectionDragging{false};

      float vpOffsetY{};
      glm::vec2 mousePos{};
      glm::vec2 mousePosStart{};
      glm::vec2 mousePosClick{};
      glm::vec2 selectionStart{};
      glm::vec2 selectionEnd{};

      std::shared_ptr<Renderer::Mesh> meshGrid{};
      Renderer::Object objGrid{};

      std::shared_ptr<Renderer::Mesh> meshLines{};
      Renderer::Object objLines{};

      std::shared_ptr<Renderer::Mesh> meshSprites{};
      Renderer::Object objSprites{};

      bool showGrid{true};
      bool showCollMesh{false};
      bool showCollObj{true};

      int gizmoOp{0};
      bool gizmoTransformActive{false};

      void onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene);
      void onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass);
      void onPostRender(Renderer::Scene& renderScene);

    public:
      Viewport3D();
      ~Viewport3D();

      std::shared_ptr<Renderer::Mesh> getLines() {
        return meshLines;
      }

      std::shared_ptr<Renderer::Mesh> getSprites() {
        return meshSprites;
      }

      void draw();
  };
}
