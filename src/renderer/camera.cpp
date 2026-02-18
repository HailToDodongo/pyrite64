/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "camera.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
//#include "glm/gtx/quaternion.hpp"

namespace
{
  constexpr glm::vec3 WORLD_UP{0,1,0};
  constexpr glm::vec3 WORLD_FORWARD{0,0,-1};
  constexpr float ORTHO_SIZE = 310.0f;
}

Renderer::Camera::Camera() {
  rot = glm::rotate(
    glm::identity<glm::quat>(),
    glm::radians(-180.0f),
    glm::vec3(1,0,0)
  );
  posOffset = {0,220,220};
}

void Renderer::Camera::update() {
  pos += velocity;
  pivot += velocity;
  velocity *= 0.9f;
  
  if (fabs(zoomSpeed) < 0.01) {
    return;
  } 

  float camDist = glm::length(posOffset);
  glm::vec3 forward = rot * WORLD_FORWARD * zoomSpeed;
  if (zoomSpeed < 0 || camDist > fabs(zoomSpeed)) {
    posOffset += forward;
  } else {
    pos += forward;
    pivot += forward;
  }
  zoomSpeed *= 0.9f;
}

void Renderer::Camera::apply(UniformGlobal &uniGlobal)
{
  float aspect = screenSize.x / screenSize.y;
  float near = 10.0f;
  float far = 10'000.0f;
  float fov = glm::radians(70.0f);

  if(isOrtho)
  {
    uniGlobal.spriteSize = {10, 10};
    float orthoSize = ORTHO_SIZE;
    uniGlobal.projMat = glm::ortho(
      -orthoSize * aspect,
      orthoSize * aspect,
      -orthoSize,
      orthoSize,
      -far, far
    );
  } else
  {
    uniGlobal.spriteSize = {7000, 7000};
    uniGlobal.projMat = glm::perspective(fov, aspect, near, far);
  }

  const glm::vec3 direction = glm::normalize(rot * WORLD_FORWARD);
  const glm::vec3 dynamicUp = glm::normalize(rot * WORLD_UP);
  const glm::vec3 target = pos + posOffset + direction;
  uniGlobal.cameraMat = glm::lookAt(pos + posOffset, target, dynamicUp);


/*
  uniGlobal.cameraMat = glm::mat4_cast(rot);
  uniGlobal.cameraMat = glm::translate(uniGlobal.cameraMat, -pos * rot);
  */
}

void Renderer::Camera::rotateDelta(glm::vec2 screenDelta)
{
  if (!isRotating) {
    rotBase = rot;
    isRotating = true;
  }

  constexpr float rotSpeed = 0.0025f;
  // rotate based on screen delta
  float angleX = screenDelta.x * -rotSpeed;
  float angleY = screenDelta.y * -rotSpeed;
  glm::quat qx = glm::angleAxis(angleX, glm::vec3(0, 1, 0));
  glm::quat qy = glm::angleAxis(angleY, glm::vec3(1, 0, 0));
  rot = qx * rotBase * qy;
  rot = glm::normalize(rot);

}

void Renderer::Camera::lookDelta(glm::vec2 screenDelta)
{
  if (!isRotating) {
    pivotBase = pivot;
  }

  rotateDelta(screenDelta);
  glm::vec3 diff = (pos + posOffset) - pivotBase;
  pivot = pos - (rot * glm::inverse(rotBase) * diff) + posOffset;
}

void Renderer::Camera::orbitDelta(glm::vec2 screenDelta)
{
  if (!isRotating) {
    posBase = pos;
  }

  rotateDelta(screenDelta);
  glm::vec3 diff = (posBase + posOffset) - pivot;
  pos = pivot + (rot * glm::inverse(rotBase) * diff) - posOffset;
}

void Renderer::Camera::moveDelta(glm::vec2 screenDelta) {
  if (!isMoving) {
    posBase = pos;
    isMoving = true;
  }

  float pixelsToWorld = 0.001f;
  if (isOrtho) {
    if (screenSize.y > 0.0f) {
      pixelsToWorld = (ORTHO_SIZE * 2.0f) / screenSize.y;
    }
  } else {
    float dist = glm::length(posOffset);
    if (dist > 0.001f) {
      pixelsToWorld = dist * 0.001f;
    }
  }

  float moveX = screenDelta.x * pixelsToWorld;
  float moveY = screenDelta.y * -pixelsToWorld;

  glm::vec3 right = rot * glm::vec3(1, 0, 0);
  glm::vec3 up = rot * glm::vec3(0, 1, 0);
 
  glm::vec3 diff = pivot - pos;
  pos = posBase + (right * moveX) + (up * moveY);
  pivot = pos + diff;
}

void Renderer::Camera::focus(glm::vec3 position, float distance) {
  isMoving = false;
  isRotating = false;
  pivot = position;
  posOffset = rot * -WORLD_FORWARD * distance;
  pos = position + posOffset;
}

