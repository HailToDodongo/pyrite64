/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  template<typename T, uint32_t SIZE>
  struct RingBuffer
  {
    T data[SIZE]{};
    uint32_t pos{0};

    void fill(const T &val) {
      for(auto &v : data)v = val;
    }

    void push(const T &val) {
      data[pos] = val;
      pos = (pos + 1) % SIZE;
    }

    T average() const {
      if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>) {
        T sum{};
        for(auto &val : data)sum += val;
        return sum / SIZE;
      } else {
        return T{};
      }
    }

    T& operator[](uint32_t idx) {
      return data[(pos + idx) % SIZE];
    }

    const T& operator[](uint32_t idx) const {
      return data[(pos + idx) % SIZE];
    }

    uint32_t size() const {
      return SIZE;
    }
  };
}