#pragma once
#include "script/userScript.h"

namespace P64::User
{
  struct ControllerData
  {
    fm_vec3_t camPosCur;
    fm_vec3_t camTargetCur;
    fm_vec3_t camForward; // persistent yaw=0 forward in world space, kept perpendicular to body up
    fm_vec3_t lastVel;
    float moveSpeedFactor;
    float coyoteTimer;
    float camYaw;
    float camYawTarget;
    float camPitch;
    float camPitchTarget;
    bool planetGravity;
    fm_vec3_t currentUp;

    void jump(Object& obj);
  };
}
