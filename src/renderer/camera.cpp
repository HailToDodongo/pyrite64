/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "camera.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"

namespace
{
  constexpr glm::vec3 WORLD_UP{0,1,0};
  constexpr glm::vec3 WORLD_FORWARD{0,0,-1};
  constexpr float ORTHO_SIZE = 310.0f;
  constexpr float FOV = 70.0f;
}

Renderer::Camera::Camera() {
  rot = glm::rotate(
    glm::identity<glm::quat>(),
    glm::radians(-180.0f),
    glm::vec3(1,0,0)
  );
  focus(glm::vec3(0,220,0), 220);
}

void Renderer::Camera::update() {
  pos += velocity;
  pivot += velocity;
  velocity *= 0.9f;
  
  if (fabs(zoomSpeed) < 0.01) {
    return;
  } 

  float camDist = glm::length(pos - pivot);
  glm::vec3 forward = rot * WORLD_FORWARD * zoomSpeed;
  if (zoomSpeed < 0 || camDist > fabs(zoomSpeed)) {
    pos += forward;
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
  float fov = glm::radians(FOV);

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
  const glm::vec3 target = pos + direction;
  uniGlobal.cameraMat = glm::lookAt(pos, target, dynamicUp);
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
  glm::vec3 diff = pos - pivotBase;
  pivot = pos - rot * glm::inverse(rotBase) * diff;
}

void Renderer::Camera::orbitDelta(glm::vec2 screenDelta)
{
  if (!isRotating) {
    posBase = pos;
  }

  rotateDelta(screenDelta);
  glm::vec3 diff = posBase - pivot;
  pos = pivot + rot * glm::inverse(rotBase) * diff;
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
    float dist = glm::length(pivot - pos);
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
  glm::vec3 posOffset = rot * -WORLD_FORWARD * distance;
  pos = pivot + posOffset;
}

void Renderer::Camera::focusSelection(Context &ctx) {
  const auto& selectedUUIDs = ctx.getSelectedObjectUUIDs();
  if (selectedUUIDs.empty()) return;
  
  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene) return;

  Utils::AABB aabb;
  for (uint32_t uuid : selectedUUIDs) {
    auto obj = scene->getObjectByUUID(uuid);
    if (!obj) continue;

    Utils::AABB objAABB = obj->getWorldAABB();
    aabb.addPoint(objAABB.min);
    aabb.addPoint(objAABB.max);
  }
  
  glm::vec3 h = aabb.getHalfExtend();
  glm::vec3 up  = rot * glm::vec3{0, 1, 0};
  float height = glm::abs(glm::dot(up, glm::vec3(h.x, 0, 0))) +
                 glm::abs(glm::dot(up, glm::vec3(0, h.y, 0))) +
                 glm::abs(glm::dot(up, glm::vec3(0, 0, h.z)));
  
  glm::vec3 fwd = rot * glm::vec3{0, 0, 1};
  float depth = glm::abs(glm::dot(fwd, glm::vec3(h.x, 0, 0))) +
                glm::abs(glm::dot(fwd, glm::vec3(0, h.y, 0))) +
                glm::abs(glm::dot(fwd, glm::vec3(0, 0, h.z)));

  float fov = glm::radians(FOV);
  float dist = 1.1f * depth + (height) / tanf(fov * 0.5f);
  focus(aabb.getCenter(), dist);
}