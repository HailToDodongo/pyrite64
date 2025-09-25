/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3d.h>

namespace P64 {
  namespace MatrixManager {
    void reset();

    T3DMat4FP* alloc(uint32_t count = 1);
    void free(T3DMat4FP* mat, uint32_t count = 1);

    uint32_t getTotalCapacity();
    bool isUsed(uint32_t index);
  }

  struct BuffMat4FP {
    T3DMat4FP *mat{};
    inline BuffMat4FP() { mat = MatrixManager::alloc(3); }
    inline ~BuffMat4FP() { MatrixManager::free(mat, 3); }

    T3DMat4FP *operator[](uint32_t idx) const { return mat + idx; }
  };

  struct RingMat4FP {
    constexpr static uint32_t COUNT = 3;

    T3DMat4FP *mat{};
    uint32_t idx{0};
    inline RingMat4FP() { mat = MatrixManager::alloc(COUNT); }
    inline ~RingMat4FP() { MatrixManager::free(mat, COUNT); }

    void next() { idx = (idx + 1) % 3; }
    [[nodiscard]] T3DMat4FP* get() const { return mat + idx; }

    [[nodiscard]] T3DMat4FP* getNext() {
      next();
      return get();
    }

    void fillSRT(const T3DVec3 &scale, const T3DVec3 &rot, const T3DVec3 &trans) {
      for(uint32_t i=0; i<COUNT; ++i) {
        t3d_mat4fp_from_srt_euler(getNext(), scale, rot, trans);
      }
    }
  };
}