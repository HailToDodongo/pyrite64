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

    Coll::RaycastRes floorCast;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete)
    {
      return;
    }

    if (obj.getComponent<Comp::Constraint>()) {
      data->noCast = 1;
    }

    if(data->noCast == 0)
    {
      data->floorCast = obj.getScene().getCollision().raycast(
        obj.pos + fm_vec3_t{0.0f, 5.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}
      );
      if(data->floorCast.hasResult()) {
        obj.pos.y = data->floorCast.hitPos.y + DEF_FLOOR_DIST;
      }
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
        data->floorCast = obj.getScene().getCollision().raycast(obj.pos, {0.0f, -1.0f, 0.0f});
      }
    }

    if(!data->floorCast.hasResult())return;
    User::DropShadows::addShadow(
      data->floorCast.hitPos,
      data->floorCast.normal,
      0.275f, 0.8f
    );
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(!event.otherBCS)return;

    if(event.otherBCS->obj->id != User::ctx.controlledId)return;
    ++User::ctx.coins;

    auto sfx = AudioManager::play2D("sfx/CoinGet.wav64"_asset);
    sfx.setVolume(0.3f);
    sfx.setSpeed(1.0f - Math::rand01()*0.1f);

    obj.getScene().addObject("ParticlesCoin.pf"_asset, obj.pos);
    obj.remove();
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    if(event.type == 1)
    {
      debugf("Event: Coin collected by id %ld\n", event.value);
      obj.getScene().addObject("ParticlesCoin.pf"_asset, obj.pos);
    }
  }
}
