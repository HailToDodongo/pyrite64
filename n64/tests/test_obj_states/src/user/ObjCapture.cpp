#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "./globals.h"
#include "./CaptureData.h"

namespace P64::Script::CB7F8F3EFD4E4EBF
{
  P64_DATA_TYPE(User::CaptureData);

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void init(Object& obj, Data *data)
  {
    //debugf("Object %d got INIT (s:%d p:%d)\n", obj.id, obj.flags & 0b01, obj.flags & 0b10);
    User::addEvent(obj, User::EvType::INIT);
  }

  void destroy(Object& obj, Data *data)
  {
    User::addEvent(obj, User::EvType::DESTROY);
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    User::addEvent(obj, User::EvType::UPDATE);
    data->countUpdate();
  }

  void fixedUpdate(Object& obj, Data *data, float fixedDeltaTime)
  {
    User::addEvent(obj, User::EvType::FIXED_UPDATE);
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    User::addEvent(obj, User::EvType::DRAW);
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    switch(event.type)
    {
      case EVENT_TYPE_READY: // object is fully initialized, no update call has happened yet
        //debugf("Object %d got READY event (s:%d p:%d)\n", obj.id, obj.flags & 0b01, obj.flags & 0b10);
        User::addEvent(obj, User::EvType::EV_READY, event.value);
      break;
      case EVENT_TYPE_ENABLE: // object got enabled
        User::addEvent(obj, User::EvType::EV_ENABLE, event.value);
      break;
      case EVENT_TYPE_DISABLE: // object got disabled
        User::addEvent(obj, User::EvType::EV_DISABLE, event.value);
      break;

      // you can check for your own custom types here too
    }
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    
  }
}
