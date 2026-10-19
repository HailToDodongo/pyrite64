#pragma once
#include "script/userScript.h"

namespace P64::User
{
  struct CaptureData
  {
    [[P64::Name("Marker")]]
    uint32_t marker = 42;

    uint32_t updates;

    void countUpdate() { ++updates; }
  };
}
