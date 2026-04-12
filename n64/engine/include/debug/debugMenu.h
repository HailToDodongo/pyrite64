/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include "menu.h"

/**
 * APIs for the builtin debug overlay.
 */
namespace P64::Debug::Overlay
{
  /**
   * Shows/Hides the debug menu.
   */
  void toggle();

  /**
   * Adds a custom entry into the overlay.
   * Note that the entire menu is reset after each scene-load, including custom entries.
   *
   * @param name must be unique and not conflict with existing entries
   * @return reference to the added menu. use this to add options / values
   */
  Menu& addCustomMenu(const char* name);

  /**
   * Removes an entry added via addCustomMenu.
   * @param name
   */
  void removeCustomMenu(const char* name);
}