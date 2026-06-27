/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <string>
#include "json.hpp"

#include "../../../renderer/camera.h"
#include "../../../renderer/vertBuffer.h"
#include "../../../renderer/framebuffer.h"
#include "../../../renderer/mesh.h"
#include "../../../renderer/object.h"
#include "../../../renderer/skeleton.h"
#include "../../../utils/container.h"

namespace Editor
{
  class Viewport3D
  {
    private:
      Renderer::UniformGlobal uniGlobal{};
      Renderer::Skeleton dummySkeleton;
      Renderer::Framebuffer fb{};
      Renderer::Camera camera{};
      uint32_t passId{};
      bool detached{false};

      bool isMouseHover{false};
      bool isMouseDown{false};
      Utils::RequestVal<uint32_t> pickedObjID{};
      bool pickAdditive{false};
      bool selectionPending{false};
      bool selectionDragging{false};
      bool cameraDragActive{false};
      bool cameraDragFlush{false};

      float moveSpeedModifier{1.0f};
      float vpOffsetY{};
      glm::vec2 cursorLockPos{};
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
      bool showIcons{true};
      bool iconsVisible{true}; 
      bool cleanPreview{false};

      // Mirror the editor view onto a scene camera (0 = free fly).
      uint64_t boundCameraUUID{0};
      bool useCameraRes{false};
      float fbScale{1.0f}; // framebuffer pixels per displayed pixel (used for object picking)

      int gizmoOp{0};
      bool gizmoTransformActive{false};
      bool overRotGizmo{false};
      bool inputActive{false};    // this viewport captured the camera drag (started inside it)
      bool viewGizmoOwned{false}; // this viewport owns the active orientation-cube drag

      void onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene);
      void onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass);
      void onPostRender(Renderer::Scene& renderScene);

      // Toggles dragging which hides the cursor for infinite movement
      void setCameraDrag(bool active);

    public:
      uint32_t winId{0};

      explicit Viewport3D(uint32_t winId = 0);
      ~Viewport3D();

      void detach();

      // Stable per-instance ImGui window title; the "###" id keeps docking state across renames.
      std::string getWindowTitle() const {
        if (winId == 0) return "3D-Viewport";
        return "3D-Viewport " + std::to_string(winId + 1) + "###vp" + std::to_string(winId);
      }

      bool isViewHovered() const { return isMouseHover; }

      nlohmann::json saveState() const;
      void loadState(const nlohmann::json &j);

      std::shared_ptr<Renderer::Mesh> getLines() {
        return meshLines;
      }

      std::shared_ptr<Renderer::Mesh> getSprites() {
        return meshSprites;
      }

      /**
       * Moves the focused object to the position of the 3D viewport camera and with the same rotation.
       */
      bool alignFocusedObjectToCamera();

      void draw();
  };
}
