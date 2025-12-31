/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <string>

namespace P64::AssetManager
{
  void init();
  void freeAll();

  void* getByIndex(uint32_t idx);
}

namespace P64
{
  /**
   * Wrapper for assets to enable auto-loading and setting it in the editor.
   * This does not create any overhead in size, as a the pointer is used to
   * store either the index (if < 0xFFFF) or the actual pointer.
   *
   * The first call to get() will resolve the index to a pointer.
   * @tparam T asset type (e.g. sprite_t)
   */
  template<typename T>
  struct AssetRef
  {
    // direct pointer, only use if you know the asset is already loaded
    T* ptr{};

    /**
     * Getter that auto-loads the asset if needed.
     * @return pointer to the asset
     */
    T* get()
    {
      if((uint32_t)ptr < 0xFFFF) {
        ptr = (T*)AssetManager::getByIndex((uint32_t)ptr);
      }
      return ptr;
    }
  };
}