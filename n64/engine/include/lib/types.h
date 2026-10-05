/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#ifndef PLATFORM_PC
  #include <libdragon.h>
#endif

namespace P64
{
  #ifndef PLATFORM_PC
    template<typename T>
    constexpr T* unached(T *ptr) {
      return (T*)UncachedAddr(ptr);
    }
  #endif


  constexpr uint32_t crc32Runtime(const char* str, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
      crc ^= static_cast<uint8_t>(str[i]);
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ 0xEDB88320;
        } else {
          crc >>= 1;
        }
      }
    }
    return ~crc;
  }

  consteval uint32_t crc32(const char* str, size_t len) {
    return crc32Runtime(str, len);
  }

  consteval uint64_t crc64(const char* str, size_t len)
  {
    uint64_t crc = 0xFFFFFFFFFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
      crc ^= static_cast<uint8_t>(str[i]);
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ 0xC96C5795D7870F42;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }
}

consteval uint32_t operator ""_crc32(const char *str, size_t len)
{
  return P64::crc32(str, len);
}

consteval uint32_t operator ""_hash(const char *str, size_t len)
{
  return P64::crc32(str, len);
}

static_assert("abcd"_crc32 == 0xed82cd11);
static_assert("someVeryLongValueToHash1234"_crc32  == 0xf92e9c5f);

consteval uint64_t operator ""_crc64(const char *str, size_t len) {
  return P64::crc64(str, len);
}

static_assert("abcd0123"_crc64 == 10979894622067171079llu);
static_assert("abcd012"_crc64 == 4087111044808462530llu);

consteval float operator ""_square(long double x) { return static_cast<float>(x * x); }

consteval float operator ""_s(long double x) { return static_cast<float>(x); }
consteval float operator ""_ms(long double x) { return static_cast<float>(x / 1000.0); }
consteval float operator ""_deg(long double x) { return static_cast<float>(x * 3.14159265358979323846 / 180.0); }
consteval float operator ""_rad(long double x) { return static_cast<float>(x); }

#define CLASS_NO_COPY_MOVE(cls) \
  cls(const cls&) = delete; \
  cls& operator=(const cls&) = delete; \
  cls(cls&&) = delete; \
  cls& operator=(cls&&) = delete;
