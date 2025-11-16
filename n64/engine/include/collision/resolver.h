/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once

#include "mesh.h"
#include "shapes.h"

namespace Coll
{
  bool sphereVsSphere(BCS &collA, BCS &collB);
  bool sphereVsBox(BCS &sphere, BCS &box);
  bool boxVsBox(BCS &boxA, BCS &boxB);
}