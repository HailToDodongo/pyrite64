/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  struct Code
  {
    // @TODO: only store used functions
    Script::FuncObjData funcInit{};
    Script::FuncObjDataDelta funcUpdate{};
    Script::FuncObjDataDelta funcDraw{};
    Script::FuncObjDataEvent funcOnEvent{};
    Script::FuncObjData funcDestroy{};

    static uint32_t getAllocSize(uint16_t* initData)
    {
      auto dataSize = Script::getCodeSizeByIndex(initData[0]);
      return sizeof(Code) + dataSize;
    }

    static void initDelete([[maybe_unused]] Object& obj, Code* data, uint16_t* initData)
    {
      if (initData == nullptr)return;

      auto scriptPtr = Script::getCodeByIndex(initData[0]);
      auto dataSize = Script::getCodeSizeByIndex(initData[0]);
      // reserved: initData[1];

      data->funcInit = scriptPtr.init;
      data->funcUpdate = scriptPtr.update;
      data->funcDraw = scriptPtr.draw;
      data->funcDestroy = scriptPtr.destroy;
      data->funcOnEvent = scriptPtr.onEvent;

      if (dataSize > 0) {
        memcpy((char*)data + sizeof(Code), (char*)&initData[2], dataSize);
      }

      if(data->funcInit) {
        char* funcData = (char*)data + sizeof(Code);
        data->funcInit(obj, funcData);
      }
    }

    static void update(Object& obj, Code* data, float deltaTime) {
      char* funcData = (char*)data + sizeof(Code);
      if(data->funcUpdate)data->funcUpdate(obj, funcData, deltaTime);
    }

    static void draw(Object& obj, Code* data, float deltaTime) {
      char* funcData = (char*)data + sizeof(Code);
      if(data->funcDraw)data->funcDraw(obj, funcData, deltaTime);
    }

    static void onEvent(Object& obj, Code* data, const ObjectEvent& event) {
      char* funcData = (char*)data + sizeof(Code);
      if(data->funcOnEvent)data->funcOnEvent(obj, funcData, event);
    }
  };
}