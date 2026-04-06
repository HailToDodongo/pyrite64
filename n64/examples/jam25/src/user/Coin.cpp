#include <vi/swapChain.h>

#include "script/userScript.h"

#include "../p64/assetTable.h"
#include "systems/context.h"
#include "systems/dropShadows.h"
#include "systems/sprites.h"
#include "scene/components/constraint.h"

namespace
{
  constexpr float DEF_FLOOR_DIST = 28.0f;
  constexpr float MAX_SHADOW_DIST = 640.0_square;
}

namespace P64::Script::CFEEDEA8CF251F94
{
  P64_DATA(
    [[P64::Name("No Cast")]]
    uint32_t noCast;

    Coll::RaycastHit floorCast;
    bool didCastFloorRay;
  );

  void init(Object& obj, Data *data)
  {
    if (obj.getComponent<Comp::Constraint>()) {
      data->noCast = 1;
    }
    data->didCastFloorRay = false;
    if (data->noCast == 0)
    {
      Coll::Raycast ray;
      ray = Coll::Raycast::create(obj.pos + fm_vec3_t{0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f, Coll::RaycastColliderTypeFlags::ALL, false, 0x08);
      obj.getScene().getCollision().raycast(ray, data->floorCast);
      if (data->floorCast.didHit)
      {
        obj.pos.y = data->floorCast.point.y + DEF_FLOOR_DIST;
      }
      data->didCastFloorRay = true;
    }
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    // always draw sprite...
    uint16_t seed = (uint32_t)data;
    User::Sprites::coin->add(obj.pos, seed);

    // check player X/Z distance for shadow
    float diff[2] {
      User::ctx.playerPos.x - obj.pos.x,
      User::ctx.playerPos.z - obj.pos.z
    };
    float dist2 = diff[0]*diff[0] + diff[1]*diff[1];
    if(dist2 > MAX_SHADOW_DIST)return;

    if(obj.getComponent<Comp::Constraint>())
    {
      // only update shadow every other frame
      if((User::ctx.frame & 0b1) == (obj.id & 0b1)) {
        Coll::Raycast ray;
        ray = Coll::Raycast::create(obj.pos, {0.0f, -1.0f, 0.0f}, 100.0f, Coll::RaycastColliderTypeFlags::ALL, false, 0x08);
        obj.getScene().getCollision().raycast(ray, data->floorCast);
      }
    }

    if(!data->floorCast.didHit)return;
    User::DropShadows::addShadow(
      data->floorCast.point,
      data->floorCast.normal,
      0.275f, 0.8f
    );
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(!event.otherObject)return;

    if(event.otherObject->id != User::ctx.controlledId)return;
    ++User::ctx.coins;

    auto sfx = AudioManager::play2D("sfx/CoinGet.wav64"_asset);
    sfx.setVolume(0.3f);
    sfx.setSpeed(1.0f - Math::rand01()*0.1f);

    obj.getScene().addObject("ParticlesCoin"_prefab, obj.pos);
    obj.remove();
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    if(event.type == 1)
    {
      debugf("Event: Coin collected by id %ld\n", event.value);
      obj.getScene().addObject("ParticlesCoin"_prefab, obj.pos);
    }
  }
}
