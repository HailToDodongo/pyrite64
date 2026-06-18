/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "hash.h"

#include <limits>
#include <random>

namespace
{
  std::random_device rd{};
  std::mt19937 e{rd()};
  std::uniform_int_distribution<uint64_t> dis(
    std::numeric_limits<std::uint64_t>::min(),
    std::numeric_limits<std::uint64_t>::max()
  );
}

uint64_t Utils::Hash::randomU64()
{
  return dis(e);
}
