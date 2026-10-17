/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "../../../../project/assets/material.h"

namespace Editor::TextureEditor
{
  /**
   * Draws the texture settings of a material slot.
   * @param modelUUID model the material belongs to (0 = none). The texture picker opens in that model's folder.
   */
  void draw(Project::Assets::MaterialTex &tex, uint64_t modelUUID = 0);

  /**
   * Texture field with a folder-based picker popup (thumbnails, breadcrumbs, search).
   * Opens in startDir (project-relative, e.g. "assets/characters/hero"), or "assets" if empty.
   * Accepts image assets dragged in from the asset browser.
   * @return true if the selection changed
   */
  bool drawTexturePicker(const char *label, uint64_t &texUUID, const std::string &startDir);
}
