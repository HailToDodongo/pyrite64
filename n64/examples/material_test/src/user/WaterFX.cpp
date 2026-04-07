#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/components/model.h"

namespace P64::Script::CE7EE79EDF010B41
{
    P64_DATA(
    float scrollA;
    float scrollB;
  );

  void init(Object& obj, Data *data)
  {
    data->scrollA = rand() % 128;
    data->scrollB = rand() % 128;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->scrollA = fmodf(data->scrollA + deltaTime*5.9f, 256.0f);
    data->scrollB = fmodf(data->scrollB + deltaTime*9.2f, 256.0f);

    auto &mat = obj.getComponent<Comp::Model>(1)->getMatInstance();

    // Crystal
    auto ph = mat.getPlaceholder(0);
    ph->tile.setOffset(0, data->scrollA);
    ph->update();

    ph = mat.getPlaceholder(1);
    ph->tile.setOffset(0, data->scrollB);
    ph->update();


    // Water
    ph = mat.getPlaceholder(2);
    ph->tile.setOffset(data->scrollA, data->scrollA);
    ph->update();

    ph = mat.getPlaceholder(3);
    ph->tile.setOffset(256 - data->scrollB, 256 - data->scrollB);
    ph->update();
  }
}
