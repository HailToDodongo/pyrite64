/**
 * outline.h
 * Pyrite64 N64 Engine — Outline Component
 *
 * @copyright 2025 - Max Bebök
 * @license MIT
 *
 * Manages per-object outline rendering using the back-face hull technique.
 * Works in conjunction with renderer/outline.h for the actual drawing.
 *
 * Binary layout (written by compOutline.cpp build()):
 *   color_t  color;       // 4 bytes RGBA
 *   float    thickness;   // 4 bytes
 *   uint8_t  mode;        // 1 byte (0=silhouette, 1=contour)
 *   uint8_t  enabled;     // 1 byte
 *   uint16_t padding;     // 2 bytes
 */
#pragma once

#include "lib/types.h"
#include "scene/object.h"
#include "renderer/outline.h"

namespace P64::Component
{
  struct OutlineComp
  {
    static constexpr uint32_t ID = 11;

    struct InitData {
      color_t  color;
      float    thickness;
      uint8_t  mode;
      uint8_t  enabled;
      uint16_t padding;
    };

    OutlineConf conf{};

    static uint32_t getAllocSize() { return sizeof(OutlineComp); }

    static void initDelete(void *mem, Object &obj, const void *initData) {
      if (!initData) {
        // Destructor path
        auto *comp = static_cast<OutlineComp*>(mem);
        if (comp->conf.hullModel) {
          outline_free_hull(comp->conf.hullModel);
          comp->conf.hullModel = nullptr;
        }
        return;
      }

      auto *data = static_cast<const InitData*>(initData);
      auto *comp = new(mem) OutlineComp();

      comp->conf.color     = data->color;
      comp->conf.thickness = data->thickness;
      comp->conf.mode      = data->mode;
      comp->conf.enabled   = data->enabled != 0;
      comp->conf.hullModel = nullptr; // baked later when model is loaded

      // If the object has a model component, bake the hull
      // This happens in the scene loader after all components are init'd
    }

    static void update(void *mem, Object &obj, float dt) {
      // Outline is purely visual — no per-frame logic
    }

    static void draw(void *mem, Object &obj) {
      auto *comp = static_cast<OutlineComp*>(mem);
      if (!comp->conf.enabled || !comp->conf.hullModel) return;

      // The outline is drawn by the render pipeline before the normal mesh
      // The pipeline queries the component and calls outline_draw_hull()
      outline_draw_hull(&comp->conf, obj.getModelMatrix());
      outline_end_hull();
    }
  };
}
