/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "script/scriptTable.h"

namespace Coll
{
  struct CollEvent;
}

namespace P64::Comp
{
  struct Code
  {
    static constexpr uint32_t ID = 0;

    static constexpr uint32_t FN_UPDATE = 1 << 0;
    static constexpr uint32_t FN_DRAW   = 1 << 1;
    static constexpr uint32_t FN_EVENT  = 1 << 2;
    static constexpr uint32_t FN_COLL   = 1 << 3;

    // store direct pointer to avoid lookup each time
    Script::ScriptEntry *script;

    // store bitmask instead of checking function.
    // this avoids one level of indirection (+mem lookup) if a function is not set.
    // the latter is very likely since many scripts will not have a "draw" function for example.
    uint32_t usedFunctions;

    inline void* getCodeData() {
      return (char*)this + sizeof(Code);
    };

    static uint32_t getAllocSize(uint16_t* initData)
    {
      auto dataSize = Script::getCodeSizeByIndex(initData[0]);
      return sizeof(Code) + dataSize;
    }

    static void initDelete([[maybe_unused]] Object& obj, Code* data, uint16_t* initData)
    {
      if (initData == nullptr)
      {
        if(data->script->destroy) {
          data->script->destroy(obj, data->getCodeData());
        }
        return;
      }

      data->usedFunctions = 0;
      data->script = &Script::getCodeByIndex(initData[0]);

      auto dataSize = Script::getCodeSizeByIndex(initData[0]);
      if (dataSize > 0) {
        memcpy(data->getCodeData(), (char*)&initData[2], dataSize);
      }

      if(data->script->update) data->usedFunctions |= FN_UPDATE;
      if(data->script->draw) data->usedFunctions |= FN_DRAW;
      if(data->script->onEvent) data->usedFunctions |= FN_EVENT;
      if(data->script->onColl) data->usedFunctions |= FN_COLL;

      if(data->script->init) {
        data->script->init(obj, data->getCodeData());
      }
    }

    static void update(Object& obj, Code* data, float deltaTime) {
      if(data->usedFunctions & FN_UPDATE) {
        data->script->update(obj, data->getCodeData(), deltaTime);
      }
    }

    static void draw(Object& obj, Code* data, float deltaTime) {
      if(data->usedFunctions & FN_DRAW) {
        data->script->draw(obj, data->getCodeData(), deltaTime);
      }
    }

    static void onEvent(Object& obj, Code* data, const ObjectEvent& event) {
      if(data->usedFunctions & FN_EVENT) {
        data->script->onEvent(obj, data->getCodeData(), event);
      }
    }

    static void onColl(Object& obj, Code* data, const Coll::CollEvent& event) {
      if(data->usedFunctions & FN_COLL) {
        data->script->onColl(obj, data->getCodeData(), event);
      }
    }
  };
}
