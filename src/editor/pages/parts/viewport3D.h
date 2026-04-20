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
#include "../../../project/component/components.h"
#include "../../../utils/container.h"

namespace Editor
{
  class Viewport3D
  {
    private:
      Renderer::UniformGlobal uniGlobal{};
      Renderer::Framebuffer fb{};
      Renderer::UniformGlobal previewUniGlobal{};
      Renderer::Framebuffer fbPreview{};
      Renderer::Camera camera{};
      uint32_t passId{};

      bool isMouseHover{false};
      bool isMouseDown{false};
      Utils::RequestVal<uint32_t> pickedObjID{};
      bool pickAdditive{false};
      bool selectionPending{false};
      bool selectionDragging{false};

      float moveSpeedModifier{1.0f};
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
      bool showCameraPreview{false};
      uint32_t previewCameraUUID{0};
      glm::vec2 previewScreenSize{};

      int gizmoOp{0};
      bool gizmoTransformActive{false};

      /**
       * Renders the scene into the provided framebuffer using either editor or in-game style overlays.
       * @param cmdBuff GPU command buffer used for the render pass.
       * @param renderScene Renderer scene that owns the active pipelines.
       * @param targetFb Framebuffer that receives the rendered image.
       * @param targetUni Global uniforms used for this pass.
       * @param drawEditorHelpers True to draw editor-only helpers and overlays.
       */
      void renderScenePass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene, Renderer::Framebuffer &targetFb, Renderer::UniformGlobal &targetUni, bool drawEditorHelpers);

      /**
       * Updates the cached camera preview state for the currently focused object.
       * @param obj Currently focused object in the viewport.
       * @param currSize Visible size of the viewport image.
       * @param scene Loaded scene used to configure the preview framebuffer.
       */
      void updateCameraPreviewState(const std::shared_ptr<Project::Object> &obj, const ImVec2 &currSize, Project::Scene *scene);

      /**
       * Draws the camera preview overlay on top of the viewport when a preview framebuffer is available.
       * @param currPos Screen position of the viewport image.
       * @param currSize Visible size of the viewport image.
       */
      void drawCameraPreviewOverlay(const ImVec2 &currPos, const ImVec2 &currSize);

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
