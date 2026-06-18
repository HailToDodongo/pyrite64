/**
 * @copyright 2026 - Max Bebök
 * @license MIT
 */
#pragma once
#include "scene/object.h"
#include "collision/characterBody.h"

namespace P64::Comp
{
  struct CharBody
  {
    static constexpr uint32_t ID = 12;

    private:
    Coll::CharacterBody body{nullptr};

    public:
    Coll::CharacterBody& getBody() {
      return body;
    }

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData) {
      return sizeof(CharBody);
    }

    static void initDelete(Object& obj, CharBody* data, void* initData);
    static void update(Object& obj, CharBody* data, float deltaTime);
  };
}
