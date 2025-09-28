/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "SHA256.h"

namespace Utils::Hash
{
  inline uint64_t sha256_64bit(const std::string& str)
  {
    SHA256 sha{};
    sha.update(str);
    auto hash = sha.digest();
    uint64_t res = 0;
    for (int i = 0; i < 8; i++) {
      res = (res << 8) | hash[i];
    }
    return res;
  }
}
