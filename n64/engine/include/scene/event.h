/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <vector>

namespace P64
{
  constexpr uint16_t EVENT_TYPE_ENABLE  = 0xFFFF - 0;
  constexpr uint16_t EVENT_TYPE_DISABLE = 0xFFFF - 1;
  constexpr uint16_t EVENT_TYPE_READY   = 0xFFFF - 2;

  // Safe ranges for user-defined custom events
  constexpr uint16_t EVENT_TYPE_CUSTOM_START = 0x0000;
  constexpr uint16_t EVENT_TYPE_CUSTOM_END   = 0xF000;

  struct ObjectEvent
  {
    uint16_t senderId{};
    uint16_t type{};
    uint32_t value{};
  };

  struct ObjectEventWrapper
  {
    ObjectEvent event{};
    uint16_t targetId{};
  };

  struct ObjectEventQueue
  {
    static constexpr uint32_t DEF_EVENT_SIZE = 64;

    std::vector<ObjectEventWrapper> events{};

    ObjectEventQueue() {
      events.reserve(DEF_EVENT_SIZE);
    }

    void add(uint16_t targetId, uint16_t senderId, uint16_t type, uint32_t value) {
      events.emplace_back(ObjectEvent{
        .senderId = senderId,
        .type = type,
        .value = value
      }, targetId);
    }

    void clear() {
      events.clear();
      if(events.capacity() > DEF_EVENT_SIZE) {
        events.shrink_to_fit();
      }
    }
  };
}