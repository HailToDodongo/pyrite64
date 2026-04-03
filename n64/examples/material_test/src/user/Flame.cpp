#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/components/model.h"

namespace P64::Script::C751DA2F182978DE
{
  P64_DATA(
    float scroll;
  );

  void init(Object& obj, Data *data)
  {
    data->scroll = rand() % 128;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->scroll = fmodf(data->scroll + deltaTime*32.0f, 256.0f);

    auto &mat = obj.getComponent<Comp::Model>()->getMatInstance();

    auto ph = mat.getPlaceholder(0);
    if(!ph)return;
    //ph->setOffset(((uint32_t)data) & 0b11111, 256 - data->scroll);
    ph->tile.setOffset(0, 256 - data->scroll);
    ph->update();
  }
}
