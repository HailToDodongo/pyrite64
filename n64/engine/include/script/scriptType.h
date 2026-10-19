/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

namespace P64::Script
{
  /**
   * Unique tag per script data type, only its address is used.
   * Inline variables share one address across all files
   */
  template<typename T>
  inline constexpr char TYPE_TAG{};
}
