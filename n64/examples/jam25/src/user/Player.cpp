#include <debug/debugDraw.h>

#include "script/userScript.h"
#include "systems/context.h"
#include "globals.h"

#include "scene/components/charBody.h"
#include "scene/components/animModel.h"
#include "systems/dropShadows.h"
#include "systems/sprites.h"
#include "collision/gfxScale.h"
#include "../p64/assetTable.h"

namespace
{
  constexpr float MOVE_SPEED = 1.7f;
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
  constexpr float JUMP_IMPULSE = 2.6f;
  constexpr float JUMP_HOLD_BOOST = 2.8f;
  constexpr float FALL_GRAVITY_MULT = 1.625f;

  void spawnParticles(const fm_vec3_t pos, uint32_t count, uint32_t seed, float dist, float size) {

    for(uint32_t i=0; i<count; ++i) {
      auto pt = pos;
      pt.x += (P64::Math::rand01()-0.5f) * dist;
      pt.z += (P64::Math::rand01()-0.5f) * dist + 0.2f;
      P64::User::Sprites::dust->add(pt, seed+i, P64::Math::rand01() * 0.2f + size);
    }
  }

}

namespace P64::Script::C17EA8EAB6CF1DEB
{
  P64_DATA(
    fm_vec3_t lastMoveDir;
    fm_vec3_t moveInputWorld;
    fm_vec3_t lastVel;          // smoothed horizontal velocity, fed into body.inputVelocity
    fm_vec3_t hurtVelocity;     // decaying knockback (horizontal only, vertical via setVelocity)
    fm_vec3_t lastSafePos;      // gfx-space respawn point

    float targetMoveYaw;
    float moveInputStrength;

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
    float blinkTimer;
    float noiseTimer;
    float targetAnimBlend;

    Coll::CharacterBody* body;
    Coll::RaycastHit shadowCast;  // downward cast used for drop-shadow positioning
    Comp::AnimModel *anim;

    fm_vec3_t lastFramePos;
    uint8_t isJumpEnd;
    uint8_t isMidJump;
    uint8_t hasHitFloor;
    uint8_t jumpHeld;
    uint8_t jumpRequested;
    uint8_t wasOnFloor;

    uint8_t stepSFXCooldown;
    uint8_t landSFXCooldown;
  );

  void init(Object& obj, Data *data)
  {
    sys_hw_memset((void*)data, 0, sizeof(Data));

    auto cb_comp = obj.getComponent<Comp::CharBody>();
    data->body = &cb_comp->getBody();

    data->camPitch = 0.31f;
    data->lastSafePos = obj.pos;

    User::ctx.controlledId = obj.id;
    User::ctx.healthTotal = 16;
    User::ctx.health = User::ctx.healthTotal;
    User::ctx.coins = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    if(data->anim == nullptr) {
      data->anim = obj.getComponent<Comp::AnimModel>();
      data->anim->setMainAnim(1);
      data->anim->setBlendAnim(0);
    }

    User::ctx.playerPos = obj.pos;

    if(obj.id != User::ctx.controlledId) return;


    {
      // Respawn when fallen out of the world
      if(obj.pos.y * Coll::getInvGfxScale() < -10.0f)
      {
        data->body->teleport(data->lastSafePos);
        data->hurtVelocity = {};
      }

      bool moveOnFloor = data->body->isOnFloor() && !data->body->isOnSteepSurface();
      bool canJump = moveOnFloor || data->inAirTime < (1.0f / 60.0f * 4);

      if(moveOnFloor) {
        data->isMidJump = false;
        data->inAirTime = 0.0f;
      } else {
        data->inAirTime += deltaTime;
      }

      const fm_vec3_t up = data->body->getSettings().up;

      // Vertical: jump start / variable height
      fm_vec3_t v = data->body->getVelocity();
      float vUp = fm_vec3_dot(&v, &up);

      if(vUp < 0.0f) data->isJumpEnd = true;
      if(vUp > 0.01f && !data->isJumpEnd && !data->jumpHeld) data->isJumpEnd = true;

      if(!data->isJumpEnd && data->jumpHeld) {
        v = v + up * (JUMP_HOLD_BOOST * deltaTime);
        data->body->setVelocity(v);
      }

      if(canJump && data->jumpRequested) {
        v = data->body->getVelocity();
        v = v + up * (std::exp(1.0f / 60.0f * deltaTime) * JUMP_IMPULSE);
        data->body->setVelocity(v);
        data->isJumpEnd = false;
        data->isMidJump = true;

        auto sfx = AudioManager::play2D("sfx/PlayerJump00.wav64"_asset);
        sfx.setSpeed(1.0f - (P64::Math::rand01() * 0.1f));
        sfx.setVolume(0.35f);
      }

      // Stylized arc: past the peak / after release, add extra downward gravity
      if(data->isJumpEnd && !moveOnFloor) {
        const float baseG = data->body->getSettings().gravity;
        v = data->body->getVelocity();
        v = v - up * (baseG * (FALL_GRAVITY_MULT - 1.0f) * deltaTime);
        data->body->setVelocity(v);
      }

      // Horizontal: smoothed input velocity
      data->lastVel.x *= MOVE_SPEED_SLOWDOWN;
      data->lastVel.z *= MOVE_SPEED_SLOWDOWN;

      if(data->moveInputStrength > 0.05f) {
        fm_vec3_t moveDir = data->moveInputWorld;
        if(data->hurtTimeout > 0.0f) {
          moveDir.x *= 0.75f;
          moveDir.z *= 0.75f;
        }
        data->lastVel.x += moveDir.x * MOVE_SPEED;
        data->lastVel.z += moveDir.z * MOVE_SPEED;
      }

      // Decaying horizontal knockback from hurts
      data->lastVel.x += data->hurtVelocity.x;
      data->lastVel.z += data->hurtVelocity.z;
      data->hurtVelocity *= 0.8f;

      data->body->inputVelocity = data->lastVel;
      data->jumpRequested = 0;

      data->body->moveAndSlide(deltaTime);

      // Publish what we're standing on so platforms can react (no collision events for char bodies).
      User::ctx.playerFloorId = data->body->floorObjectId();
    }

    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    auto inp = joypad_get_inputs(JOYPAD_PORT_1);
    auto held = joypad_get_buttons_held(JOYPAD_PORT_1);

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

      model->getMatInstance().colorPrim = {
        (uint8_t)(blickColor.x * 255),
        (uint8_t)(blickColor.y * 255),
        (uint8_t)(blickColor.z * 255),
        0xFF
      };

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

    bool onFloor = data->body->isOnFloor() && !data->body->isOnSteepSurface();

    data->jumpHeld = inp.btn.a;
    if(pressed.a) {
      data->jumpRequested = 1;
    }

    bool isFocus = held.z;
    if(isFocus)
    {
      // align camera yaw with player
      data->camYawTarget = data->targetMoveYaw + T3D_PI;
    }

    // orbit controls for camera (C-buttons)
    data->camPitchVelocity *= ROT_SPEED_SLOWDOWN;

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
    fm_quat_from_euler_zyx(&camRot, T3D_PI - data->camPitch, data->camYaw, 0.0f);

    // Determine cam target, this is slightly ahead of the player in the direction facing.
    fm_vec3_t camTarget = obj.pos + fm_vec3_t{0.0f, 20.0f, 0.0f};

    fm_vec3_t yawVec{};
    yawVec.x = sinf(data->camTargetOffsetYaw);
    yawVec.y = 0.0f;
    yawVec.z = cosf(data->camTargetOffsetYaw);

    auto targetOffset = yawVec * 20.0f;
    fm_vec3_lerp(&data->camTargetOffset, &data->camTargetOffset, &targetOffset, 0.1f);

    camTarget += data->camTargetOffset;
    data->camTarget.x = camTarget.x;
    data->camTarget.z = camTarget.z;

    // LERP Y differently: if we jump drap very slowly behind to not be too jarring.
    // Once grounded, snap to it faster (e.g. falling from platform down to ground)
    float lerpY = onFloor ? CAM_TARGET_LERP_Y_GROUND : CAM_TARGET_LERP_Y;
    data->camTarget.y = fm_lerp(data->camTarget.y, camTarget.y, lerpY);

    fm_vec3_t camOffset{0.0f, CAMERA_HEIGHT, CAMERA_DISTANCE};
    camOffset.z += camPitchNorm * 150;
    camOffset = camRot * camOffset;
    fm_vec3_t camPos = data->camTarget - camOffset;

    auto &cam = SceneManager::getCurrent().getActiveCamera();
    cam.setLookAt(camPos, data->camTarget);

    fm_vec3_t moveInput{inp.stick_x/100.0f, 0.0f, inp.stick_y/100.0f};
    if (moveInput.x > 1.0f) moveInput.x = 1.0f;
    if (moveInput.x < -1.0f) moveInput.x = -1.0f;
    if (moveInput.z > 1.0f) moveInput.z = 1.0f;
    if (moveInput.z < -1.0f) moveInput.z = -1.0f;
    float stickLen = fm_vec3_len(&moveInput);
    data->moveInputStrength = stickLen;
    data->moveInputWorld = {};

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

	    //Prevent diagonal movement from being faster than cardinal directions
	    if (float l = sqrtf(moveInput.z * moveInput.z + moveInput.x * moveInput.x); l > 1.0f) {
	      moveInput.z = moveInput.z / l;
	      moveInput.x = moveInput.x / l;
	    }
	  
	    fm_vec3_t moveDir = camForward * moveInput.z + camRight * moveInput.x;
      //fm_vec3_norm(&moveDir, &moveDir);

	    data->lastMoveDir = moveDir;
      data->moveInputWorld = moveDir;
    }

    // update animation state
    if (data->isMidJump) {
      t3d_anim_set_speed(data->anim->getMainAnim(), 0.2f);
    }

    float blendSpeed = data->targetAnimBlend > 0.5f ? 0.3f : 0.09f;
    blendSpeed *= deltaTime * 60.0f;
    data->anim->blendFactor = t3d_lerp(data->anim->blendFactor, data->targetAnimBlend, blendSpeed);

    // Rotate player to face movement direction
    float currYaw = isFocus ? data->targetMoveYaw : atan2f(data->lastMoveDir.x, data->lastMoveDir.z);

    data->targetMoveYaw = t3d_lerp_angle(data->targetMoveYaw, currYaw, MOVE_YAW_LERP);
    data->camTargetOffsetYaw = t3d_lerp_angle(data->camTargetOffsetYaw, currYaw, CAM_OFFSET_YAW_LERP);

    fm_quat_from_euler_zyx(&obj.rot, 0.0f, data->targetMoveYaw, 0.0f);
    fm_quat_norm(&obj.rot, &obj.rot);

    // Only bank a respawn point on a floor that isn't carrying us (static surface).
    bool floorMoved = data->body->wasMovedByFloor();

    bool playerStill = data->lastFramePos.x == obj.pos.x
                    && data->lastFramePos.y - obj.pos.y < 0.01f
                    && data->lastFramePos.z == obj.pos.z;

    if(playerStill && !floorMoved)
    {
      data->notMovingTime += deltaTime;
    } else {
      data->notMovingTime = 0;
    }

    if(onFloor && data->notMovingTime > 30.0_ms) {
      data->lastSafePos = obj.pos;
    }

    // SFX- Hit floor impact (landed this frame on a walkable surface)
    bool justLanded = onFloor && !data->wasOnFloor;
    if (justLanded) {
      if (data->landSFXCooldown == 0) {
        auto sfx = AudioManager::play2D("sfx/StepStone00.wav64"_asset);
        sfx.setSpeed(1.0f - (P64::Math::rand01() * 0.21f));
        sfx.setVolume(0.45f);
        data->landSFXCooldown = 30;
      }
      data->hasHitFloor = 1;
    } else if(onFloor) {
      if(data->hasHitFloor < 2) ++data->hasHitFloor;
    } else {
      data->hasHitFloor = 0;
    }
    data->wasOnFloor = onFloor;

    if(data->landSFXCooldown > 0)--data->landSFXCooldown;

    // FX (Dust)
    if(onFloor && data->dustTimer < 0.0f)
    {
      data->dustTimer = 0.1f + Math::rand01() * 0.3f;
      auto seed = (uint32_t)rand();
      spawnParticles(data->body->getFootPos(), seed % 3 + 1, seed, 40.0f, 0.5f);
    }

    data->lastFramePos = obj.pos;

    // Dynamic Materials
    auto &mat = data->anim->getMatInstance();
    auto ph = mat.getPlaceholder(0);

    data->noiseTimer = fmodf(data->noiseTimer + deltaTime, 1024.0f);
    mat.colorPrim.a = (fm_sinf(data->noiseTimer) * 0.5f + 0.5f) * 0x70;

    data->blinkTimer -= deltaTime;
    if(data->hurtTimeout > 0.0f) {
      ph->tile.setTexture("face02.i4.sprite"_asset);
    } else {
      ph->tile.setTexture(data->blinkTimer > 0.0f
        ? "face00.i4.sprite"_asset
        : "face01.i4.sprite"_asset
      );
    }
    if(data->blinkTimer < -0.15f) {
      data->blinkTimer = 2.0f + Math::rand01()*1.0f;
    }

    ph->update();
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
      }
    }
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(!event.hitCollider) return;

    if(event.hitCollider->writeMask() & User::COLL_LAYER_HURT)
    {
      if (data->hurtTimeout <= 0.0f) {
        auto posDiff = event.selfCollider->worldCenter() - event.hitCollider->worldCenter();
        fm_vec3_norm(&posDiff, &posDiff);

        // Horizontal knockback decays via hurtVelocity → inputVelocity
        data->hurtVelocity = posDiff * 4.0f;
        data->hurtVelocity.y = 0.0f;

        // Vertical kick applied as a single impulse on the body's velocity
        const fm_vec3_t up = data->body->getSettings().up;
        fm_vec3_t v = data->body->getVelocity();
        v = v + up * 0.4f;
        data->body->setVelocity(v);
      }

      hurt(obj, data, 1);
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    // drop shadow: when grounded, take the floor straight from the body,
    // when airborne, do an additional cast down
    fm_vec3_t floorPos;
    fm_vec3_t floorNormal;
    bool haveFloor;
    if(data->body->isOnFloor())
    {
      floorPos    = data->body->getFootPos();
      floorNormal = data->body->floorNormal();
      haveFloor   = true;
    } else {
      Coll::Raycast ray = Coll::Raycast::create(
        obj.pos * Coll::getInvGfxScale(), {0.0f, -1.0f, 0.0f}, 5.0f,
        Coll::RaycastColliderTypeFlags::ALL, false, 0x08);
      SceneManager::getCurrent().getCollision().raycast(ray, data->shadowCast);
      floorPos    = data->shadowCast.point * P64::Coll::getGfxScale();
      floorNormal = data->shadowCast.normal;
      haveFloor   = data->shadowCast.didHit;
    }

    if(haveFloor)
    {
      float floorY = floorPos.y;
      float shadowHeight = obj.pos.y - floorY;
      shadowHeight *= 0.001f;
      shadowHeight = Math::clamp(shadowHeight, 0.0f, 1.0f);
      shadowHeight = 1.0f - shadowHeight;
      User::DropShadows::addShadow(
          {obj.pos.x, floorY, obj.pos.z},
          floorNormal,
          0.55f * shadowHeight,
          1.0f);
    }

    DrawLayer::use2D();
    DrawLayer::useDefault();
  }
}
