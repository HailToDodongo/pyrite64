#include "script/userScript.h"
#include <libdragon.h>
#include <vector>

namespace P64::User
{
  enum class EvType : uint32_t {
    INIT,
    DESTROY,
    UPDATE,
    FIXED_UPDATE,
    DRAW,
    EV_READY,
    EV_ENABLE,
    EV_DISABLE
  };

  constexpr const char* const EVENT_NAMES[] = {
    "INIT",
    "DESTROY",
    "UPDATE",
    "FIXED_UPDATE",
    "DRAW",
    "EV_READY",
    "EV_ENABLE",
    "EV_DISABLE"
  };

  struct EvCapture
  {
    EvType type{};
    uint32_t frame{};

    uint16_t objId{};
    uint16_t parentId{};
    bool enabled{true};
    uint32_t value{};

    EvCapture event(EvType newType) const {
      EvCapture ev = *this;
      ev.type = newType;
      return ev;
    }
    EvCapture disabled() const {
      EvCapture ev = *this;
      ev.enabled = false;
      return ev;
    }

  };

  extern uint32_t currFrame;
  extern std::vector<EvCapture> events;

  inline void addEvent(P64::Object &obj, EvType type, uint32_t value = 0)
  {
    events.push_back({
      .type = type,
      .frame = currFrame,
      .objId = obj.id,
      .parentId = obj.group,
      .enabled = obj.isEnabled(),
      .value = value,
    });
  }
}