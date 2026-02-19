/**
 * cel_shader.h
 * Pyrite64 — N64 Cartoon Render Module API
 */

#pragma once

#include <stdint.h>
#include <libdragon.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the cel shader. Must be called once before use. */
void cel_shader_init(void);

/** Free cel shader resources. */
void cel_shader_cleanup(void);

/**
 * Set up RDP state for cel-shaded rendering of the next mesh.
 * @param bands  Shade bands (2=stark contrast, 4=classic toon, 8=smooth)
 * @param color  Base tint (use RGBA(255,255,255,255) for no tint)
 */
void cel_shader_begin(uint8_t bands, color_t color);

/** Restore RDP state after cel-shaded mesh. */
void cel_shader_end(void);

/**
 * Utility: quantize a diffuse value to N discrete bands.
 * Used in editor asset pipeline to pre-bake cartoon lighting.
 */
float cel_quantize(float diffuse, uint8_t bands);

#ifdef __cplusplus
}
#endif
