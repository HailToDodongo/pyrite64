#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/object.h"
#include "scene/components/charBody.h"
#include "debug/debugMenu.h"
#include "debug/debugDraw.h"

namespace
{
  constexpr float JUMP_SPEED  = 8.0f;  // Initial up-axis speed on jump
  constexpr float COYOTE_TIME = 0.15f;  // Grace window after leaving the floor
  float MOVE_SPEED  = 0.009f;

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

  constinit uint64_t ticks{0};
  constexpr fm_vec3_t PLANET_POS{0, 1300, 0};
}

namespace P64::Script::CD0A328E7EE01313
{
  P64_DATA(
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
    data->camForward      = {0.0f, 0.0f, 1.0f};
    data->lastVel = {};
    data->moveSpeedFactor = 1.0f;
    data->planetGravity = false;
    data->currentUp    = {0.0f, 1.0f, 0.0f};

    Debug::Overlay::addCustomMenu("Game")
      .add("Speed",   MOVE_SPEED, 0.001f, 0.04f, 0.001f);
  }

  void destroy(Object& obj, Data *data) {}

  void update(Object& obj, Data *data, float deltaTime) {
    auto inp     = joypad_get_inputs(JOYPAD_PORT_1);
    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    auto &body = obj.getComponent<P64::Comp::CharBody>()->getBody();

    if(pressed.r) {
      data->planetGravity = !data->planetGravity;
    }

    constexpr float UP_TRANSITION_SPEED = 0.15f;
    fm_vec3_t targetUp;
    if(data->planetGravity) {
      auto relPos =  obj.pos - PLANET_POS;
      fm_vec3_norm(&targetUp, &relPos);
    } else {
      targetUp = {0.0f, 1.0f, 0.0f};
    }
    fm_vec3_lerp(&data->currentUp, &data->currentUp, &targetUp, UP_TRANSITION_SPEED);
    fm_vec3_norm(&data->currentUp, &data->currentUp);
    body.setUp(data->currentUp);

    const fm_vec3_t up = body.getSettings().up;
    fm_vec3_t forward0 = data->camForward - up * fm_vec3_dot(&data->camForward, &up);
    float fwdLen2 = fm_vec3_len2(&forward0);
    if(fwdLen2 < 1e-4f) {
      fm_vec3_t seed = {0.0f, 0.0f, 1.0f};
      if(fabsf(fm_vec3_dot(&up, &seed)) > 0.99f) seed = {1.0f, 0.0f, 0.0f};
      forward0 = seed - up * fm_vec3_dot(&up, &seed);
      fm_vec3_norm(&forward0, &forward0);
    } else {
      forward0 = forward0 * (1.0f / sqrtf(fwdLen2));
    }
    data->camForward = forward0;

    fm_vec3_t right0;
    fm_vec3_cross(&right0, &up, &forward0);

    // Align the object's visual rotation so its local +Y matches body up.
    constexpr fm_vec3_t WORLD_Y = {0.0f, 1.0f, 0.0f};
    float upDotY = fm_vec3_dot(&WORLD_Y, &up);
    if(upDotY > 0.9999f) {
      obj.rot = P64::Math::QUAT_IDENTITY;
    } else if(upDotY < -0.9999f) {
      constexpr fm_vec3_t FLIP_AXIS = {1.0f, 0.0f, 0.0f};
      fm_quat_from_axis_angle(&obj.rot, &FLIP_AXIS, T3D_PI);
    } else {
      fm_vec3_t axis;
      fm_vec3_cross(&axis, &WORLD_Y, &up);
      fm_vec3_norm(&axis, &axis);
      fm_quat_from_axis_angle(&obj.rot, &axis, acosf(upDotY));
    }

    // Camera controls
    if(pressed.c_right) data->camYawTarget -= CAM_YAW_SNAP;
    if(pressed.c_left)  data->camYawTarget += CAM_YAW_SNAP;
    if(inp.btn.c_up)   data->camPitchTarget -= CAM_PITCH_SPEED * deltaTime;
    if(inp.btn.c_down) data->camPitchTarget += CAM_PITCH_SPEED * deltaTime;

    // Clamp pitch target to prevent looking directly from above and going below floor.
    data->camPitchTarget = fmaxf(CAM_PITCH_MIN, fminf(CAM_PITCH_MAX, data->camPitchTarget));

    data->camYaw = fm_lerp(data->camYaw, data->camYawTarget, CAM_INTERP_SPEED);
    data->camPitch = fm_lerp(data->camPitch, data->camPitchTarget, CAM_INTERP_SPEED);

    // Move relative to camera yaw, in the plane perpendicular to body up.
    // player_forward = -radial, player_right is radial rotated -90° around up.
    float sx = inp.stick_x, sy = inp.stick_y;
    float cy = fm_cosf(data->camYaw), sn = fm_sinf(data->camYaw);
    fm_vec3_t targetVelocity =
      right0   * ((sx * cy - sy * sn) * MOVE_SPEED)
      - forward0 * ((sx * sn + sy * cy) * MOVE_SPEED);
    data->lastVel *= 0.8f;
    data->lastVel += targetVelocity * data->moveSpeedFactor;

    // force respawn when falling down too much
    if(obj.pos.y < -750.0f) body.teleport({0, 100, 0});

    const bool grounded = body.isOnFloor();
    const fm_vec3_t bodyUp = body.getSettings().up;

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

    ticks = get_ticks();
    body.moveAndSlide(deltaTime);
    ticks = get_ticks() - ticks;

    if(body.isOnSteepSurface()) {
      data->moveSpeedFactor *= 0.7f;
    } else {
      data->moveSpeedFactor = fminf(1.0f, data->moveSpeedFactor + 2.0f * deltaTime);
    }

    // Lerp target toward player. Split into the up-axis component (slower in air)
    // and the perpendicular plane (XZ-equivalent) so behavior is the same as the
    // original Y-up case, just rotated with body up.
    float camUpInterp = body.isOnFloor() ? CAM_POS_INTERP_Y_GROUND : CAM_POS_INTERP_Y_AIR;
    fm_vec3_t targetDelta = obj.pos - data->camTargetCur;
    float deltaUp = fm_vec3_dot(&targetDelta, &up);
    fm_vec3_t deltaPerp = targetDelta - up * deltaUp;
    data->camTargetCur = data->camTargetCur
      + deltaPerp * CAM_POS_INTERP_XZ
      + up * (deltaUp * camUpInterp);

    // Radial direction (target → camera at pitch=0) and the camera offset.
    // The (radial, up) plane is tilted by camPitch, plus a constant lift along up.
    float pitch_cos = fm_cosf(data->camPitch);
    float pitch_sin = fm_sinf(data->camPitch);
    fm_vec3_t radial = right0 * sinf(data->camYaw) + forward0 * cosf(data->camYaw);
    data->camPosCur = data->camTargetCur
      + radial * (pitch_cos * CAM_DIST)
      + up * (CAM_HEIGHT - pitch_sin * CAM_DIST);

    auto &cam = obj.getScene().getActiveCamera();
    cam.setLookAt(data->camPosCur, data->camTargetCur, up);

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

/*
    Debug::printf(posX, posY, "Pos : %+.3f %+.3f %+.3f",
      obj.pos.x,
      obj.pos.y,
      obj.pos.z
    );
    posY += 9;
    Debug::printf(posX, posY, "Velo: %.1f %.1f %.1f",
      body.getVelocity().x,
      body.getVelocity().y,
      body.getVelocity().z
    );

    posY += 9;
    float normSteepness = acosf(body.floorNormal().y) * (180.0f / Math::PI);
    Debug::printf(posX, posY, "Norm: %.2f %.2f %.2f (%.1f deg)",
      body.floorNormal().x,
      body.floorNormal().y,
      body.floorNormal().z,
      normSteepness
    );
*/
    posY = 240 - 16;
    Debug::printf(posX, posY, "State: %s %s",
      body.isOnFloor() ? "Floor" : "  -  ",
      body.isOnSteepSurface() ? "Steep" : "  -  "
    );
    //posY -= 9;
    //Debug::printf(posX, posY, "T: %lldus", TICKS_TO_US(ticks));
    posY -= 9;
    Debug::printf(posX, posY, "[R] Planet: %s", data->planetGravity ? "On " : "Off");

    rdpq_mode_pop();

    Debug::isMonospace = false;
    DrawLayer::useDefault();
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
  }
}
