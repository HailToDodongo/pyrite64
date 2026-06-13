/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::User
{
  struct Context
  {
    uint16_t controlledId{0};

    int32_t health{0};
    uint32_t healthTotal{0};

    uint32_t coins{0};

    bool isCutscene{false};
    bool forceBars{false};
    uint32_t frame{0};

    fm_vec3_t playerPos{};
    uint16_t  playerFloorId{0}; // id of the object the player is currently standing on (0 = airborne)
  };

  extern Context ctx;
}
