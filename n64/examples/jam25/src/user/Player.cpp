#include <debug/debugDraw.h>

#include "script/userScript.h"
#include "systems/context.h"
#include "globals.h"

#include "scene/components/collBody.h"
#include "scene/components/collMesh.h"
#include "scene/components/animModel.h"
#include "systems/dropShadows.h"
#include "systems/sprites.h"
#include "collision/attach.h"
#include "systems/dialog.h"
#include "../p64/assetTable.h"

namespace
{
  constexpr float GRAVITY = 1300.0f;
  constexpr float GRAVITY_JUMP = 800.0f;

  constexpr float MOVE_SPEED = 170.0f;
  constexpr float MOVE_SPEED_SLOWDOWN = 0.4f;
  constexpr float MOVE_YAW_LERP = 0.22f;

  constexpr float CAM_TARGET_LERP_Y = 0.01f;
  constexpr float CAM_TARGET_LERP_Y_GROUND = 0.05f;
  constexpr float CAM_OFFSET_YAW_LERP = 0.11f;

  constexpr float ROT_SPEED = 0.01f;
  constexpr float ROT_SPEED_SLOWDOWN = 0.9f;

  constexpr float CAMERA_DISTANCE = 200.0f;
  constexpr float CAMERA_HEIGHT = 70.0f;

  constexpr float CAM_PITCH_MIN = -0.45f;
  constexpr float CAM_PITCH_MAX = 1.1f;

  constexpr float HURT_TIMEOUT = 1.0f;

  void spawnParticles(const fm_vec3_t pos, uint32_t count, uint32_t seed, float dist, float size) {

    for(uint32_t i=0; i<count; ++i) {
      auto pt = pos;
      pt.x += (P64::Math::rand01()-0.5f) * dist;
      pt.y -= 25.0f;
      //pt.y -= slashTimer == SLASH_TIMER_MAX ? 1.0f : 5.0f;
      pt.z += (P64::Math::rand01()-0.5f) * dist + 0.2f;
      P64::User::Sprites::dust->add(pt, seed+i, P64::Math::rand01() * 0.2f + size);
    }
  }
}

namespace P64::Script::C17EA8EAB6CF1DEB
{
  P64_DATA(
    fm_vec3_t lastMoveDir;
    float targetMoveYaw;

    fm_vec3_t hurtVelocity;
    fm_vec3_t lastFramePos;
    fm_vec3_t lastSafePos;

    fm_vec3_t camTarget;
    fm_vec3_t camTargetOffset;
    float camTargetOffsetYaw;
    float camYaw;
    float camYawTarget;
    float camPitch;

    float camPitchVelocity;

    float dustTimer;
    float inAirTime;
    float notMovingTime;
    float hurtTimeout;

    float targetAnimBlend;

    Coll::RaycastRes floorCast;
    Coll::Attach meshAttach;
    Comp::AnimModel *anim;
    uint8_t isJumpEnd;
    uint8_t isMidJump;
    uint8_t hasHitFloor;

    uint8_t stepSFXCooldown;
    uint8_t landSFXCooldown;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) return;
    sys_hw_memset((void*)data, 0, sizeof(Data));

    data->camPitch = 0.31f;
    data->lastSafePos = obj.pos;

    User::ctx.controlledId = obj.id;
    User::ctx.healthTotal = 16;
    User::ctx.health = User::ctx.healthTotal;
    User::ctx.coins = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto coll = obj.getComponent<Comp::CollBody>();
    if(data->anim == nullptr) {
      data->anim = obj.getComponent<Comp::AnimModel>();
      data->anim->setMainAnim(1);
      data->anim->setBlendAnim(0);
    }

    auto &bcs = coll->bcs;

    User::ctx.playerPos = obj.pos;

    float gravity = data->isJumpEnd ? GRAVITY : GRAVITY_JUMP;
    bcs.velocity.y -= gravity * deltaTime;

    // @TODO: respawn nicer
    if(bcs.center.y < -1000)
    {
      obj.pos = bcs.center = data->lastSafePos;
      bcs.velocity = {};
    }

    if(obj.id != User::ctx.controlledId) return;

    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    auto inp = joypad_get_inputs(JOYPAD_PORT_1);
    auto held = joypad_get_buttons_held(JOYPAD_PORT_1);

    /*if(pressed.z) // DEBUG
    {
      User::ctx.coins += 50;
    }*/

    if (data->hurtTimeout > 0.0f)
    {
      data->hurtTimeout = fmaxf(data->hurtTimeout - deltaTime, 0.0f);

      auto model = obj.getComponent<Comp::AnimModel>();

      fm_vec3_t blickColor = {
        fm_sinf((HURT_TIMEOUT - data->hurtTimeout) * 20.0f) * 0.25f + 0.75f,
         0.5f, 0.5f
      };
      fm_vec3_t colorNormal{1.0f, 1.0f, 1.0f};
      fm_vec3_lerp(&blickColor, &colorNormal, &blickColor, data->hurtTimeout / HURT_TIMEOUT);

      model->material.colorPrim.r = (uint8_t)(blickColor.x * 255);
      model->material.colorPrim.g = (uint8_t)(blickColor.y * 255);
      model->material.colorPrim.b = (uint8_t)(blickColor.z * 255);

      bcs.velocity += data->hurtVelocity;
      data->hurtVelocity *= 0.8f;

      if (data->hurtTimeout == 0.0f) {
        model->material.colorPrim = {0xFF, 0xFF, 0xFF, 0xFF};
        data->hurtVelocity = {};
      }
    }

    if(User::ctx.isCutscene || User::ctx.forceBars)
    {
      pressed = {};
      inp = {};
      held = {};
    }

    bool onFloor = bcs.hitTriTypes & Coll::TriType::FLOOR;
    bool canJump = onFloor || data->inAirTime < (1.0f / 60.0f * 4);

    data->floorCast = SceneManager::getCurrent().getCollision().raycast(bcs.center, {0.0f, -1.0f, 0.0f});
    /*float floorHeightDiff = data->floorCast.hasResult() ? (bcs.center.y - data->floorCast.hitPos.y) : 10000.0f;
    // snap to floor if close enough
    if(!data->isMidJump && floorHeightDiff < 28.0f && floorHeightDiff > -50.0f && bcs.velocity.y <= 0.0f)
    {
      bcs.center.y = data->floorCast.hitPos.y + 24.0f;
      //bcs.velocity.y = 0.0f;
    }*/

    if(onFloor)data->isMidJump = false;

    //if(inp.btn.a)bcs.velocity.y += 15.0f;

    if(!data->isJumpEnd && inp.btn.a)
    {
      bcs.velocity.y += 360.0f * deltaTime;
    }

    if(canJump && pressed.a) {
      bcs.velocity.y += std::exp(1.0f / 60.0f * deltaTime) * 270.0f;
      data->isJumpEnd = false;
      data->isMidJump = true;

      auto sfx = AudioManager::play2D("sfx/PlayerJump00.wav64"_asset);
      sfx.setSpeed(1.0f - (P64::Math::rand01() * 0.1f));
      sfx.setVolume(0.35f);
    }

    if(bcs.velocity.y < 0.0f) {
      data->isJumpEnd = true;
    }
    if(bcs.velocity.y > 1.0f && !data->isJumpEnd)
    {
      if(!inp.btn.a)data->isJumpEnd = true;
    }

    bool isFocus = held.z;
    if(isFocus)
    {
      // align camera yaw with player
      data->camYawTarget = data->targetMoveYaw + T3D_PI;
    }

    // orbit controls for camera (C-buttons)
    data->camPitchVelocity *= ROT_SPEED_SLOWDOWN;

    //data->camYawVelocity   += inp.cstick_x * -ROT_SPEED * deltaTime;
    if(pressed.c_left || pressed.c_right)
    {
      if(pressed.c_left)data->camYawTarget += T3D_PI / 4;
      if(pressed.c_right)data->camYawTarget -= T3D_PI / 4;
      // snap to nearest 45 degrees
      float snapAngle = T3D_PI / 4;
      data->camYawTarget = roundf(data->camYawTarget / snapAngle) * snapAngle;
    }

    data->camPitchVelocity += inp.cstick_y * -ROT_SPEED * deltaTime;

    data->camYaw = t3d_lerp_angle(data->camYaw, data->camYawTarget, 0.15f);
    data->camPitch = Math::clamp(data->camPitch + data->camPitchVelocity, CAM_PITCH_MIN, CAM_PITCH_MAX);

    // pull in camera more if lower to the ground, and push out when looking from top
    float camPitchNorm = (data->camPitch - CAM_PITCH_MIN) / (CAM_PITCH_MAX - CAM_PITCH_MIN);
    camPitchNorm *= camPitchNorm;
    camPitchNorm = (camPitchNorm * 0.5f) - 0.5f;

    fm_quat_t camRot;
    fm_vec3_t eulerCam{0, -data->camYaw, data->camPitch};
    fm_quat_from_euler(&camRot, eulerCam.v);

    // Determine cam target, this is slightly ahead of the player in the direction facing.
    fm_vec3_t camTarget = obj.pos + fm_vec3_t{0.0f, 20.0f, 0.0f};

    fm_vec3_t yawVec{};
    yawVec.x = sinf(data->camTargetOffsetYaw);
    yawVec.y = 0.0f;
    yawVec.z = cosf(data->camTargetOffsetYaw);

    //Debug::drawLine(obj.pos, obj.pos + yawVec * 50.0f, {0x00, 0xFF, 0x00, 0xFF});

    auto targetOffset = yawVec * 20.0f;
    fm_vec3_lerp(&data->camTargetOffset, &data->camTargetOffset, &targetOffset, 0.1f);

    camTarget += data->camTargetOffset;
    data->camTarget.x = camTarget.x;
    data->camTarget.z = camTarget.z;

    // LERP Y differently: if we jump drap very slowly behind to not be too jarring.
    // Once grounded, snap to it faster (e.g. falling from platform down to ground)
    float lerpY = onFloor ? CAM_TARGET_LERP_Y_GROUND : CAM_TARGET_LERP_Y;
    data->camTarget.y = fm_lerp(data->camTarget.y, camTarget.y, lerpY);

    //data->camTarget = camTarget;

    fm_vec3_t camOffset{0.0f, CAMERA_HEIGHT, CAMERA_DISTANCE};
    camOffset.z += camPitchNorm * 150;
    camOffset = camRot * camOffset;
    fm_vec3_t camPos = data->camTarget - camOffset;

    auto &cam = SceneManager::getCurrent().getActiveCamera();
    cam.setLookAt(camPos, data->camTarget);

    // collider attachment
    bcs.center -= data->meshAttach.update(bcs.center);

    // player physics
    bcs.velocity.x *= MOVE_SPEED_SLOWDOWN;
    bcs.velocity.z *= MOVE_SPEED_SLOWDOWN;
    //bcs.velocity = Math::clamp(bcs.velocity, -1.0f, 1.0f);

    fm_vec3_t moveInput{inp.stick_x/100.0f, 0.0f, inp.stick_y/100.0f};
    if (moveInput.x > 1.0f) moveInput.x = 1.0f;
    if (moveInput.x < -1.0f) moveInput.x = -1.0f;
    if (moveInput.z > 1.0f) moveInput.z = 1.0f;
    if (moveInput.z < -1.0f) moveInput.z = -1.0f;
    float stickLen = fm_vec3_len(&moveInput);

    data->targetAnimBlend = 1.0f;
    if(stickLen > 0.05f)
    {
      if (data->stepSFXCooldown == 0 && onFloor) {
        uint32_t sfxPool[3] {
          "sfx/StepStone00.wav64"_asset,
          "sfx/StepStone01.wav64"_asset,
          "sfx/StepStone02.wav64"_asset
        };
        auto sfx = AudioManager::play2D(sfxPool[rand() % 3]);
        sfx.setSpeed(1.0f - (P64::Math::rand01() * 0.21f));
        sfx.setVolume(0.3f);

        data->stepSFXCooldown = 10 + (rand() % 4);
      }

      if (data->stepSFXCooldown > 0)--data->stepSFXCooldown;

      data->targetAnimBlend = 0.0f;
      t3d_anim_set_speed(data->anim->getMainAnim(), stickLen * 1.5f);

      data->dustTimer -= deltaTime;

      fm_vec3_t camForward = camRot * fm_vec3_t{0.0f, 0.0f, 1.0f};
      camForward.y = 0.0f;
      fm_vec3_norm(&camForward, &camForward);

      fm_vec3_t camRight = camRot * fm_vec3_t{1.0f, 0.0f, 0.0f};
      camRight.y = 0.0f;
      fm_vec3_norm(&camRight, &camRight);

      fm_vec3_t moveDir = camForward * moveInput.z + camRight * moveInput.x;
      //fm_vec3_norm(&moveDir, &moveDir);

      if (data->hurtTimeout > 0.0f) {
        moveDir.x *= 0.75f;
        moveDir.z *= 0.75f;
      }

      data->lastMoveDir = moveDir;

      bcs.velocity.x += moveDir.x * MOVE_SPEED;
      bcs.velocity.z += moveDir.z * MOVE_SPEED;

      /*if(data->isJumpEnd && data->floorCast.hasResult())
      {
        bcs.velocity.y -= (1.0f - data->floorCast.normal.y) * 50.0f;
      }*/
    }

    // update animation state
    if (data->isMidJump) {
      t3d_anim_set_speed(data->anim->getMainAnim(), 0.2f);
    }

    float blendSpeed = data->targetAnimBlend > 0.5f ? 0.3f : 0.09f;
    blendSpeed *= deltaTime * 60.0f;
    data->anim->blendFactor = t3d_lerp(data->anim->blendFactor, data->targetAnimBlend, blendSpeed);

    // kick back from hurting
    bcs.velocity += data->hurtVelocity;
    data->hurtVelocity *= 0.8f;

    // Rotate player to face movement direction
    float currYaw = isFocus ? data->targetMoveYaw : atan2f(data->lastMoveDir.x, data->lastMoveDir.z);

    data->targetMoveYaw = t3d_lerp_angle(data->targetMoveYaw, currYaw, MOVE_YAW_LERP);
    data->camTargetOffsetYaw = t3d_lerp_angle(data->camTargetOffsetYaw, currYaw, CAM_OFFSET_YAW_LERP);

    fm_vec3_t euler{0.0f, -data->targetMoveYaw, -T3D_PI};
    fm_quat_from_euler(&obj.rot, euler.v);
    fm_quat_norm(&obj.rot, &obj.rot);

    if(data->lastFramePos.x == obj.pos.x && data->lastFramePos.z == obj.pos.z)
    {
      data->notMovingTime += deltaTime;
    } else {
      data->notMovingTime = 0;
    }

    if(onFloor) {
      if (data->hasHitFloor < 2) {
        ++data->hasHitFloor;
      }
      data->inAirTime = 0;
      if(data->notMovingTime > 30.0_ms) {
        data->lastSafePos = obj.pos;
      }
    } else {
      data->hasHitFloor = 0;
      data->inAirTime += deltaTime;
    }

    // SFX- Hit floor impact
    if (data->hasHitFloor == 1) {
      if (data->landSFXCooldown == 0) {
        auto sfx = AudioManager::play2D("sfx/StepStone00.wav64"_asset);
        sfx.setSpeed(1.0f - (P64::Math::rand01() * 0.21f));
        sfx.setVolume(0.45f);
        data->landSFXCooldown = 30;
      }
    }

    if(data->landSFXCooldown > 0)--data->landSFXCooldown;

    // FX (Dust)
    if(onFloor && data->dustTimer < 0.0f)
    {
      data->dustTimer = 0.1f + Math::rand01() * 0.3f;
      auto seed = (uint32_t)rand();
      spawnParticles(bcs.center, seed % 3 + 1, seed, 40.0f, 0.5f);
    }

    data->lastFramePos = obj.pos;
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {

  }

  void hurt(Object& obj, Data *data, int units)
  {
    if(User::ctx.health > 0 && data->hurtTimeout <= 0.0f)
    {
      User::ctx.health -= units;
      data->hurtTimeout = HURT_TIMEOUT;

      if(User::ctx.health <= 0)
      {
        User::ctx.health = 0;
        User::ctx.isCutscene = true;
        //User::ScreenFade::fadeOut(2, 3.0f);
        //User::ctx.respawnTimer = 3.0f;
      }
    }
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(event.otherMesh)
    {
      data->meshAttach.setReference(event.otherMesh);
      return;
    }

    if(!event.otherBCS)return;

    if(event.otherBCS->maskWrite & User::COLL_LAYER_HURT)
    {
      if (data->hurtTimeout <= 0.0f) {
        auto posDiff = event.selfBCS->center - event.otherBCS->center;
        fm_vec3_norm(&posDiff, &posDiff);

        data->hurtVelocity = posDiff * 400.0f;
        data->hurtVelocity.y = 40.f;
      }

      hurt(obj, data, 1);
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    // drop shadow
    float shadowHeight = obj.pos.y - data->floorCast.hitPos.y;
    shadowHeight *= 0.001f;
    shadowHeight = Math::clamp(shadowHeight, 0.0f, 1.0f);
    shadowHeight = 1.0f - shadowHeight;

    User::DropShadows::addShadow(
      {obj.pos.x, data->floorCast.hitPos.y, obj.pos.z},
      data->floorCast.normal,
      0.55f * shadowHeight,
      1.0f
    );

    DrawLayer::use2D();

    //rdpq_text_printf(nullptr, User::FONT_SMALL, 10, 200, "P: %.2f %.2f %.2f",obj.pos.x, obj.pos.y, obj.pos.z);
    //rdpq_text_printf(nullptr, User::FONT_SMALL, 10, 210, "Yaw/Pitch: %.2f %.2f",data->camYaw, data->camPitch);
    //rdpq_text_printf(nullptr, User::FONT_SMALL, 10, 220, "Jump: %d | %.2f",data->isJumpEnd, data->inAirTime);
    //rdpq_text_printf(nullptr, User::FONT_SMALL, 10, 228, "Jumping: %d", data->isMidJump);

    /*rdpq_text_printf(nullptr, User::FONT_SMALL, 10, 110,
        "Norm: %.2f %.2f %.2f", data->floorCast.normal.x, data->floorCast.normal.y, data->floorCast.normal.z
    );*/

    /*rdpq_mode_push();
    rdpq_set_mode_fill({0,0,0,0});
    if(!data->isJumpEnd)
    {
      rdpq_fill_rectangle(32, 32, 64,64);
    }

    Debug::printStart();
    auto bcs = obj.getComponent<Comp::CollBody>();
    Debug::printf(180, 120, "V:%.2f P:%.2f\n", (double)bcs->bcs.velocity.y, obj.pos.y);

    rdpq_mode_pop();
*/
    DrawLayer::useDefault();
  }
}
