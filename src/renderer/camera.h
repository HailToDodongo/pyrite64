/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <SDL3/SDL.h>

#include "uniforms.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Renderer
{
  class Camera
  {
    private:

    public:
      glm::vec3 pos{};
      glm::vec3 pivot{};
      glm::quat rot{0,0,0,1};
      glm::vec2 screenSize{1,1};

      glm::vec3 velocity{};
      float zoomSpeed{};

      glm::quat rotBase{};
      bool isRotating{false};
      glm::vec3 posBase{};
      glm::vec3 pivotBase{};
      bool isMoving{false};
      bool isOrtho{false};

      Camera();

      void update();

      void apply(UniformGlobal &uniGlobal);

      void rotateDelta(glm::vec2 screenDelta);
      void lookDelta(glm::vec2 screenDelta);
      void orbitDelta(glm::vec2 screenDelta);
      
      void stopRotateDelta() {
        isRotating = false;
      }

      void moveDelta(glm::vec2 screenDelta);

      void stopMoveDelta() {
        isMoving = false;
      }

      void focus(glm::vec3 position, float distance);
      float calculateFocusDistance(float height);
  };
}
