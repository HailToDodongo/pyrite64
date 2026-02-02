/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

namespace P64::User::Fonts
{
  void init();
  void destroy();

  void useNumber();
  void printNumber(int x, int y, uint32_t num);

}
