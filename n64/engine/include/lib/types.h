/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  template<typename T>
  constexpr T* unached(T *ptr) {
    return (T*)UncachedAddr(ptr);
  }
}

consteval uint32_t operator"" _u32(const char *str, size_t len) {
  uint32_t result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= ((uint32_t)str[i] << (8 * (3-i)));
  }
  return result;
}

static_assert("abcd"_u32 == 0x61626364);
static_assert("abc"_u32  == 0x61626300);

consteval uint64_t operator"" _u64(const char *str, size_t len) {
  uint64_t result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= ((uint64_t)str[i] << (8 * (7-i)));
  }
  return result;
}

static_assert("abcd0123"_u64 == 0x61626364'30313233);
static_assert("abcd012"_u64 == 0x61626364'30313200);
static_assert("abc"_u64  == 0x61626300'00000000);

consteval float operator ""    _s(long double x) { return static_cast<float>(x); }
consteval float operator ""   _ms(long double x) { return static_cast<float>(x / 1000.0); }

consteval float operator ""    _s(unsigned long long x) { return static_cast<float>(x); }
consteval float operator ""   _ms(unsigned long long x) { return static_cast<float>(x) / 1000.0f; }
