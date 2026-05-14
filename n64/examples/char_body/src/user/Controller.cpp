#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/object.h"
#include "scene/components/charBody.h"
#include <debug/debugDraw.h>

namespace
{
  constexpr float JUMP_SPEED  = 8.0f;  // Initial up-axis speed on jump
  constexpr float COYOTE_TIME = 0.15f;  // Grace window after leaving the floor
  constexpr float MOVE_SPEED  = 0.009f;

  constexpr float CAM_DIST   = 390.0f;
  constexpr float CAM_HEIGHT = 400.0f;
  constexpr float CAM_YAW_SNAP = 45.0_deg;
  constexpr float CAM_PITCH_SPEED = 2.0f;  // Pitch target change speed (radians/sec)
  constexpr float CAM_INTERP_SPEED = 0.15f;  // Rotation interpolation factor per frame (0-1)
  constexpr float CAM_POS_INTERP_XZ = 0.15f;  // Camera XZ position interpolation
  constexpr float CAM_POS_INTERP_Y_AIR = 0.01f;  // Camera Y interpolation in the air
  constexpr float CAM_POS_INTERP_Y_GROUND = 0.05f;  // Camera Y interpolation when grounded
  constexpr float CAM_PITCH_MIN = -45.0_deg;
  constexpr float CAM_PITCH_MAX = 70.0_deg;
}

namespace P64::Script::CD0A328E7EE01313
{
  P64_DATA(
    fm_vec3_t camPosCur;
    fm_vec3_t camTargetCur;
    fm_vec3_t lastVel;
    float moveSpeedFactor;
    float coyoteTimer;
    float camYaw;
    float camYawTarget;
    float camPitch;
    float camPitchTarget;
  );

  void init(Object& obj, Data *data)
  {
    data->coyoteTimer     = 0.0f;
    data->camYaw          = 0.0f;
    data->camYawTarget    = 0.0f;
    data->camPitch        = 0.0f;
    data->camPitchTarget  = 0.0f;
    data->camTargetCur    = obj.pos;
    data->camPosCur       = obj.pos + fm_vec3_t{0.0f, CAM_HEIGHT, CAM_DIST};
    data->lastVel = {};
    data->moveSpeedFactor = 1.0f;
  }

  void destroy(Object& obj, Data *data) {}

  void update(Object& obj, Data *data, float deltaTime) {
    auto inp     = joypad_get_inputs(JOYPAD_PORT_1);
    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    auto &body = obj.getComponent<P64::Comp::CharBody>()->getBody();

    // Camera controls
    if(pressed.c_right) data->camYawTarget -= CAM_YAW_SNAP;
    if(pressed.c_left)  data->camYawTarget += CAM_YAW_SNAP;
    if(inp.btn.c_up)   data->camPitchTarget -= CAM_PITCH_SPEED * deltaTime;
    if(inp.btn.c_down) data->camPitchTarget += CAM_PITCH_SPEED * deltaTime;

    // Clamp pitch target to prevent looking directly from above and going below floor.
    data->camPitchTarget = fmaxf(CAM_PITCH_MIN, fminf(CAM_PITCH_MAX, data->camPitchTarget));

    data->camYaw = fm_lerp(data->camYaw, data->camYawTarget, CAM_INTERP_SPEED);
    data->camPitch = fm_lerp(data->camPitch, data->camPitchTarget, CAM_INTERP_SPEED);

    // Move relative to camera yaw: rotate stick XZ by camYaw.
    float sx = inp.stick_x, sy = inp.stick_y;
    float cy = fm_cosf(-data->camYaw), sy_ = fm_sinf(-data->camYaw);
    fm_vec3_t targetVelocity = {
      (sx * cy + sy * sy_) * MOVE_SPEED,
      0.0f,
      (sx * sy_ - sy * cy) * MOVE_SPEED
    };
    data->lastVel *= 0.8f;
    data->lastVel += targetVelocity * data->moveSpeedFactor;

    // force respawn when falling down too much
    if(obj.pos.y < -750.0f) body.teleport({0, 100, 0});

    const bool grounded = body.isOnFloor();
    const fm_vec3_t bodyUp = body.settings.up;

    body.inputVelocity = data->lastVel;

    if(grounded) {
      data->coyoteTimer = COYOTE_TIME;
    } else if(data->coyoteTimer > 0.0f) {
      data->coyoteTimer = fmaxf(0.0f, data->coyoteTimer - deltaTime);
    }

    if(pressed.a && data->coyoteTimer > 0.0f) {
      body.setVelocity(body.getVelocity() + bodyUp * JUMP_SPEED);
      data->coyoteTimer = 0.0f; // consume so we don't re-trigger mid-air
    }

    body.moveAndSlide(deltaTime);
    if(body.isOnSteepSurface()) {
      data->moveSpeedFactor *= 0.7f;
    } else {
      data->moveSpeedFactor = fminf(1.0f, data->moveSpeedFactor + 2.0f * deltaTime);
    }

    float camYInterp = body.isOnFloor() ? CAM_POS_INTERP_Y_GROUND : CAM_POS_INTERP_Y_AIR;
    data->camTargetCur.x = fm_lerp(data->camTargetCur.x, obj.pos.x, CAM_POS_INTERP_XZ);
    data->camTargetCur.y = fm_lerp(data->camTargetCur.y, obj.pos.y, camYInterp);
    data->camTargetCur.z = fm_lerp(data->camTargetCur.z, obj.pos.z, CAM_POS_INTERP_XZ);

    float pitch_cos = fm_cosf(data->camPitch);
    float pitch_sin = fm_sinf(data->camPitch);
    data->camPosCur = data->camTargetCur + fm_vec3_t{
      sinf(data->camYaw) * pitch_cos * CAM_DIST,
      CAM_HEIGHT - pitch_sin * CAM_DIST,
      cosf(data->camYaw) * pitch_cos * CAM_DIST
    };

    auto &cam = obj.getScene().getActiveCamera();
    cam.setLookAt(data->camPosCur, data->camTargetCur);

    if(inp.btn.z)
    {
      body.debugDraw();
    }
  }

  void fixedUpdate(Object& obj, Data *data, float fixedDeltaTime)
  {
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    DrawLayer::use2D();
    rdpq_mode_push();

    auto &body = obj.getComponent<P64::Comp::CharBody>()->getBody();

    Debug::printStart();
    Debug::isMonospace = true;
    uint16_t posX = 16;
    uint16_t posY = 16;
    Debug::printf(posX, posY, "Pos : %+.3f %+.3f %+.3f\n",
      obj.pos.x,
      obj.pos.y,
      obj.pos.z
    );
    posY += 9;
    Debug::printf(posX, posY, "Velo: %.1f %.1f %.1f\n",
      body.getVelocity().x,
      body.getVelocity().y,
      body.getVelocity().z
    );

    posY += 9;
    float normSteepness = acosf(body.floorNormal().y) * (180.0f / Math::PI);
    Debug::printf(posX, posY, "Norm: %.2f %.2f %.2f (%.1f deg)\n",
      body.floorNormal().x,
      body.floorNormal().y,
      body.floorNormal().z,
      normSteepness
    );

    posY = 240 - 16;
    Debug::printf(posX, posY, "State: %s %s %s\n",
      body.isOnFloor() ? "Floor" : "  -  ",
      body.isOnSteepSurface() ? "Steep" : "  -  ",
      body.didSnapToFloor() ? "FSnap" : "  -  "
    );

    rdpq_mode_pop();

    Debug::isMonospace = false;
    DrawLayer::useDefault();
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
  }
}
