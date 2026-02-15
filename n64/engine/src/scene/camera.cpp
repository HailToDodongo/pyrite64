/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "scene/camera.h"
#include "lib/logger.h"
#include "scene/globalState.h"

namespace
{
  constexpr fm_vec3_t FORWARD{0,0,-1};
  constexpr fm_vec3_t UP{0,1,0};
}

P64::Camera::Camera() {
  viewports = t3d_viewport_create_buffered(3);
}

P64::Camera::~Camera() {
  t3d_viewport_destroy(&viewports);
}

void P64::Camera::update([[maybe_unused]] float deltaTime)
{
  t3d_viewport_set_perspective(&viewports, fov, aspectRatio, near, far);
  t3d_viewport_set_view_matrix(&viewports, &viewMatrix);
}

void P64::Camera::attach() {
  t3d_viewport_attach(viewports);
}

void P64::Camera::setScreenArea(int x, int y, int width, int height) {
  t3d_viewport_set_area(viewports, x,y, width, height);
}

void P64::Camera::setLookAt(const fm_vec3_t &newPos, const fm_vec3_t &newTarget, const fm_vec3_t &newUp) {
  target = newTarget;
  up = newUp;
  pos = newPos;
  t3d_mat4_look_at(&viewMatrix, &newPos, &newTarget, &newUp);
  needsProjUpdate = true;
}

void P64::Camera::setPosRot(const fm_vec3_t &newPos, const fm_quat_t&rot) {
  setLookAt(newPos, newPos + (rot * FORWARD), rot * UP);
}

fm_vec3_t P64::Camera::getScreenPos(const fm_vec3_t &worldPos)
{
  fm_vec3_t res{};
  t3d_viewport_calc_viewspace_pos(viewports, res, worldPos);
  return res;
}

