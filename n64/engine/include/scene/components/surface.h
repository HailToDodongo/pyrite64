/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "scene/object.h"

namespace P64::Comp
{
  /**
   * Component owning one or more runtime-allocated surfaces ('surface_t').
   * These can be used as offscreen render-targets or CPU/RDP drawn textures.
   *
   * With more than one buffer, the active surface cycles each frame,
   * 'getSurface()' always returns the current frame's surface.
   */
  struct Surface
  {
    static constexpr uint32_t ID = 13;
    static constexpr uint32_t MAX_BUFFERS = 3;

    enum Flags : uint8_t
    {
      FLAG_CLEAR = 1 << 0,
      FLAG_DEPTH = 1 << 1,
    };

    struct InitData
    {
      uint16_t width;
      uint16_t height;
      uint32_t clearColor; // packed 32-bit RGBA
      uint8_t format;      // tex_format_t
      uint8_t buffCount;
      uint8_t flags;
    };

    surface_t buffers[MAX_BUFFERS]{};
    surface_t depthBuff{}; // optional, shared by all buffers
    uint64_t clearPattern{};
    uint8_t buffCount{1};
    uint8_t currIdx{};
    uint8_t flags{};

    /**
     * @return surface of the current frame
     */
    [[nodiscard]] surface_t& getSurface() { return buffers[currIdx]; }

    /**
     * @param idx buffer index, must be < 'getBufferCount()'
     * @return specific surface independent of the current frame
     */
    [[nodiscard]] surface_t& getSurface(uint32_t idx) { return buffers[idx]; }

    /**
     * @return surface of the previous frame, identical to 'getSurface()' if single-buffered
     */
    [[nodiscard]] surface_t& getPrevSurface() {
      return buffers[(currIdx + buffCount - 1) % buffCount];
    }

    [[nodiscard]] uint32_t getBufferCount() const { return buffCount; }

    /**
     * @return depth buffer of the surface, or nullptr if not enabled.
     *         Cameras targeting a surface without one re-use the main depth buffer.
     */
    [[nodiscard]] surface_t* getDepthBuffer() {
      return depthBuff.buffer ? &depthBuff : nullptr;
    }

    /**
     * Clears the current surface with the clear-color, done automatically
     * each frame if "Clear" is enabled in the editor.
     */
    void clear() { clear(buffers[currIdx]); }

    /**
     * Clears a given surface with the clear-color.
     * @param surf surface to clear, must be one of this component's buffers
     */
    void clear(const surface_t &surf) const {
      sys_hw_memset64(surf.buffer, clearPattern, surf.height * surf.stride);
    }

    static uint32_t getAllocSize([[maybe_unused]] InitData* initData)
    {
      return sizeof(Surface);
    }

    static void initDelete([[maybe_unused]] Object& obj, Surface* data, InitData* initData);

    static void update([[maybe_unused]] Object& obj, Surface* data, [[maybe_unused]] float deltaTime);
  };
}
