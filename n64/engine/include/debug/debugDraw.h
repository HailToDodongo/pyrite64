/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

/**
 * Various helpers to draw either text or lines for debugging purposes.
 * This is not intended for in-game use but rather for internal or user-defined debugging overlays.
 */
namespace P64::Debug
{
  void init();

  void drawAABB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, color_t color = {0xFF, 0xFF, 0xFF, 0xFF});
  void drawLine(const fm_vec3_t &a, const fm_vec3_t &b, color_t color = {0xFF,0xFF,0xFF,0xFF});
  color_t paletteColor(uint32_t index);
  void drawSphere(const fm_vec3_t &center, float radius, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawOBB(const fm_vec3_t &p, const fm_vec3_t &halfExtend, const fm_quat_t &rot, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawCapsule(const fm_vec3_t &p, float radius, float innerHalfHeight, const fm_quat_t &rot, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawCylinder(const fm_vec3_t &p, float radius, float halfHeight, const fm_quat_t &rot, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawCone(const fm_vec3_t &p, float radius, float halfHeight, const fm_quat_t &rot, color_t color = {0xFF,0xFF,0xFF,0xFF});
  void drawPyramid(const fm_vec3_t &p, float baseHalfWidthX, float baseHalfWidthZ, float halfHeight, const fm_quat_t &rot, color_t color = {0xFF,0xFF,0xFF,0xFF});

  void draw(surface_t *fb);

  void printStart();

  extern bool isMonospace;

  /**
   * Prints a string (no formating) to a given position.
   * NOTE: this is only intended for debugging, not for in-game text!
   *
   * @param x screen-position X
   * @param y screen-position Y
   * @param str string to print
   * @return new X position after the printed string
   */
  int print(uint16_t x, uint16_t y, const char* str);

  /**
   * Prints a string (no formating) to a given position.
   * NOTE: this is only intended for debugging, not for in-game text!
   *
   * @param x screen-position X
   * @param y screen-position Y
   * @param fmt printf-style format string
   * @param ... printf-style arguments
   * @return new X position after the printed string
   */
  int printf(uint16_t x, uint16_t y, const char *fmt, ...);

  inline void setColor(color_t col = {0xFF, 0xFF, 0xFF, 0xFF}) {
    rdpq_set_prim_color(col);
  }

  inline void setBgColor(color_t col = {0,0,0,0}) {
    rdpq_set_env_color(col);
  }

  void destroy();
}

// special character codes for icons only present in the debug font:

#define DEBUG_CHAR_SQUARE    "$"
#define DEBUG_CHAR_ARROW     "\x80"
#define DEBUG_CHAR_DIR       "\x81"
#define DEBUG_CHAR_RETURN    "\x82"
#define DEBUG_CHAR_CHECK_0FF "\x83"
#define DEBUG_CHAR_CHECK_ON  "\x84"
#define DEBUG_CHAR_US        "\x85"
#define DEBUG_CHAR_TREE      "\x86"
#define DEBUG_CHAR_TREE_END  "\x87"
#define DEBUG_CHAR_FUNC      "\x88"
#define DEBUG_CHAR_VALUE     "\x89"