#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "./ControllerData.h"

namespace P64::Script::CD6A684AD3ACE60E
{
  P64_DATA(
    [[P64::Name("Cooldown")]]
    float cooldown = 1.0f;
    float cooldownCurr = 0.0f;
  );

  void init(Object& obj, Data *data)
  {
    data->cooldownCurr = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    // this is called once every frame, put your main logic here
    if(data->cooldownCurr > 0.0f) {
      data->cooldownCurr -= deltaTime;
    }
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(!event.otherObject || data->cooldownCurr > 0.0f)return;

    auto playerScript = event.otherObject->getScriptData<User::ControllerData>();
    if(!playerScript)return;

    playerScript->jump(*event.otherObject);
    data->cooldownCurr = data->cooldown;
  }
}
