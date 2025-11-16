/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "scene/camera.h"
#include "lib/logger.h"
#include "scene/globalState.h"

void P64::Camera::update(float deltaTime)
{
  vpIdx = (vpIdx + 1) % 3;
  auto &vp = viewports[vpIdx];

  //if(needsProjUpdate) {
    t3d_viewport_set_projection(vp, fov, near, far);
    // vp._normScaleW = 0.001f;
    //needsProjUpdate = false;
  //}

  t3d_viewport_look_at(vp, pos, target, T3DVec3{{0, 1, 0}});

/*  T3DVec3 up{{0, 1, 0}};
  t3d_mat4_look_at(vp.matCamera, pos, target, up);

  t3d_mat4_mul(&vp.matCamProj, &vp.matProj, &vp.matCamera);
  t3d_mat4_to_frustum(&vp.viewFrustum, &vp.matCamProj);
  vp._isCamProjDirty = false;
  */
}

void P64::Camera::attach() {
  t3d_viewport_attach(viewports[vpIdx]);
}

