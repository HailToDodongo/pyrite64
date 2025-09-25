/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "lib/matrixManager.h"
#include "lib/logger.h"
#include "lib/types.h"

namespace {
  constexpr uint32_t MATRIX_COUNT = 128 * 3;
  static_assert(MATRIX_COUNT % 32 == 0);

  uint32_t usedFlags[MATRIX_COUNT / 32]{};
  uint32_t laseIndex = 0;
  T3DMat4FP *bufferPtr{nullptr};

  T3DMat4FP buffer[MATRIX_COUNT]{}; // mask of used matrices, each bit represents one matrix
}

void P64::MatrixManager::reset() {
  data_cache_hit_writeback(buffer, sizeof(buffer));
  bufferPtr = unached(buffer);
  laseIndex = 0;
  memset(usedFlags, 0, sizeof(usedFlags));
}

T3DMat4FP *P64::MatrixManager::alloc(uint32_t count) {
  uint32_t countMask = (1 << count) - 1;
  for(uint32_t i=0; i<MATRIX_COUNT/32; ++i) {
    uint32_t idx = (laseIndex + i) % (MATRIX_COUNT / 32);

    // we only consider contiguous blocks of free matrices
    for(uint32_t b=0; b<32-count; ++b)
    {
      uint32_t mask = countMask << b;
      if((usedFlags[idx] & mask) == 0) {
        usedFlags[idx] |= mask;
        laseIndex = idx;
        return bufferPtr + (idx * 32 + b);
      }
    }
  }

  Log::error("MatrixManager: Out of matrices!");
  return nullptr;
}

void P64::MatrixManager::free(T3DMat4FP *mat, uint32_t count) {
  uint32_t idx = (mat - bufferPtr) / 32;
  uint32_t countMask = (1 << count) - 1;
  uint32_t b = (mat - bufferPtr) % 32;
  usedFlags[idx] &= ~(countMask << b);
}

bool P64::MatrixManager::isUsed(uint32_t index) {
  return usedFlags[index / 32] & (1 << (index % 32));
}

uint32_t P64::MatrixManager::getTotalCapacity() {
  return MATRIX_COUNT;
}

