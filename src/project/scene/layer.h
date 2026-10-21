/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../../utils/hash.h"
#include "../../utils/prop.h"

namespace Project
{
  struct LayerConf
  {
    PROP_STRING(name);
    PROP_BOOL(depthCompare);
    PROP_BOOL(depthWrite);
    PROP_U32(blender);
    PROP_BOOL(fog);
    PROP_U32(fogColorMode);
    PROP_VEC4(fogColor);
    PROP_FLOAT(fogMin);
    PROP_FLOAT(fogMax);
    PROP_S32(lightMode);
  };

  /**
   * Layers are referenced by a 32-bit value, which is either an index or a hash of the name
   */
  namespace Layer
  {
    constexpr uint32_t REF_DEFAULT = 0;
    constexpr uint32_t MAX_INDEX_REF = 64;

    constexpr bool isIndexRef(uint32_t ref) {
      return ref <= MAX_INDEX_REF;
    }

    /**
     * Layer key, same as '_hash' at runtime
     */
    constexpr uint32_t keyFromName(std::string_view name) {
      return Utils::Hash::crc32(name);
    }

    /**
     * Check if a hash would collide with an index (you must be very unlucky!)
     */
    constexpr bool isValidName(std::string_view name) {
      return !isIndexRef(keyFromName(name));
    }

    /**
     * @return index of the referenced layer, or -1 if it doesn't exist
     */
    inline int findIndex(const std::vector<LayerConf> &layers, uint32_t ref)
    {
      if(isIndexRef(ref))return ref < layers.size() ? (int)ref : -1;
      for(size_t i=0; i<layers.size(); ++i) {
        if(keyFromName(layers[i].name.value) == ref)return (int)i;
      }
      return -1;
    }

    /**
     * @return reference the editor stores for the layer at the given index
     */
    inline uint32_t refFromIndex(const std::vector<LayerConf> &layers, int idx)
    {
      if(idx < 0 || idx >= (int)layers.size())return REF_DEFAULT;
      auto &name = layers[idx].name.value;
      return isValidName(name) ? keyFromName(name) : (uint32_t)idx;
    }

    /**
     * Returns the name, or an adjusted one if it wasn't unique.
     * @param ignore layer to skip, e.g. the one being renamed
     */
    inline std::string makeUniqueName(const std::vector<LayerConf> &layers, const std::string &name, const LayerConf *ignore = nullptr)
    {
      auto isUsed = [&](const std::string &n) {
        if(!isValidName(n))return true;
        for(auto &l : layers) {
          if(&l != ignore && l.name.value == n)return true;
        }
        return false;
      };

      if(!isUsed(name))return name;
      for(int i=2;; ++i) {
        auto res = name + " " + std::to_string(i);
        if(!isUsed(res))return res;
      }
    }
  }
}
