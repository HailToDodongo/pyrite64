#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "systems/context.h"
#include <scene/components/nodeGraph.h>
#include "scene/components/model.h"

namespace P64::Script::C19072097105FC85
{
  P64_DATA(
    [[P64::Name("Scene ID")]]
    uint16_t sceneId;
    [[P64::Name("No Dialog")]]
    uint16_t noDialog;

    uint16_t state;
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void init(Object& obj, Data *data)
  {
    data->state = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    fm_vec3_t rotAxis{0.0f, -1.0f, 0.0f};
    fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, deltaTime * 0.5f);
    fm_quat_norm(&obj.rot, &obj.rot);

    for(int i=0; i<2; ++i) {
      auto model = obj.getComponent<Comp::Model>(i);
      auto ph = model->getMatInstance().getPlaceholder(0);
      ph->tile.s.offset = (ph->tile.s.offset + 16) & 0xFFF;
      ph->update();
    }
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if (data->state != 0)return;
    if (!event.otherBCS || !event.otherBCS->obj)return;
    if (User::ctx.controlledId != event.otherBCS->obj->id)return;

    data->state = 1;
    obj.getComponent<Comp::NodeGraph>()->run(data->sceneId, data->noDialog);
  }
}
