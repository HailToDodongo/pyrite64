
#include "actor/base.h"
#include "scene/sceneManager.h"

namespace P64::Script::a28ec2e68
{
  void update(Actor::Base &actor, float deltaTime)
  {
    //actor.scale = {0.1f, 0.8f, 0.1f};
    fm_quat_identity(&actor.rot);

    auto btn = joypad_get_buttons(JOYPAD_PORT_1);
    auto stick = joypad_get_inputs(JOYPAD_PORT_1);

    auto &scene = SceneManager::getCurrent();
    float moveX = stick.stick_x * 0.1f * deltaTime;
    scene.getCamera().pos.x += moveX;
    scene.getCamera().target.x += moveX;

    if(btn.a) {
      SceneManager::load(scene.getId() == 0 ? 1 : 0);
    }

    actor.pos.x += deltaTime;
    //debugf("update: %f\n", actor.pos.x);
  }
  
  void draw(Actor::Base &actor, float deltaTime)
  {
  }
  
  void destroy(Actor::Base &actor)
  {

  }
}
  
  