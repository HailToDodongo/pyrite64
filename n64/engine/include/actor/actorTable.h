/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "actor/base.h"

namespace P64::Actor
{
  Base* createActor(uint16_t id, void* argBuffer);
}