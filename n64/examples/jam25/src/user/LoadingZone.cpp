#include "script/userScript.h"
#include "systems/context.h"
#include "systems/screenFade.h"

namespace P64::Script::CDFD59456D61CD17
{
  P64_DATA(
    uint32_t sceneId;
    uint32_t sceneLoadId;
  );

  void update(Object& obj, Data *data, float deltaTime)
  {
    if(data->sceneLoadId && User::ScreenFade::isDone())
    {
      SceneManager::load(data->sceneId);
      obj.remove();
    }
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(data->sceneLoadId != 0)return;
    if(event.otherBCS->obj->id != User::ctx.controlledId)return;

    data->sceneLoadId = data->sceneId;
    User::ScreenFade::fadeOut(0, 1.4f);
  }
}
