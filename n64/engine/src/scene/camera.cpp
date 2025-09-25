/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "scene/camera.h"
#include "lib/logger.h"
#include "scene/globalState.h"

namespace {
  constexpr float OVERRIDE_SPEED = 1.2f;
}

void P64::Camera::update(float deltaTime)
{
  auto &vp = viewports[vpIdx];
  vpIdx = (vpIdx + 1) % 3;

  //if(needsProjUpdate) {
    t3d_viewport_set_projection(vp, fov, near, far);
    //needsProjUpdate = false;
  //}

  t3d_viewport_look_at(vp, pos, target, T3DVec3{{0, 1, 0}});
}

void P64::Camera::attach() {
  t3d_viewport_attach(viewports[vpIdx]);
}

