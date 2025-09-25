
#include "actor/base.h"
#include "lib/assetManager.h"
#include "audio/audioManager.h"

namespace {
  wav64_t *bgm{nullptr};
}

namespace P64::Script::a98b28da1
{
  void init(Actor::Base &actor, const void *args)
  {
    debugf("INIT\n");

    bgm = AssetManager::getWAV(0x12345678, "rom:/DiscFrag.wav64");
    AudioManager::play2D(bgm);
  }
  
  void update(Actor::Base &actor, float deltaTime)
  {
    actor.pos.y += deltaTime;
  }
  
  void draw(Actor::Base &actor, float deltaTime)
  {
  }
  
  void destroy(Actor::Base &actor)
  {
  }
}
  
  