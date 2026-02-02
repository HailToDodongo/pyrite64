/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <script/nodeGraph.h>
#include "dialogMessages.h"

namespace P64::User::Dialog
{
  extern rspq_block_t *dplBox;

  void init();
  void destroy();

  // returns true if a dialog is active
  bool update();

  void draw();
  int showMessage(uint32_t messageId);

  // direct functions to draw box BG outside the dialog system
  inline void drawBoxBgSart() {
    rspq_block_run(dplBox);
  }
  void drawBoxBg(float posX, float posY, float width, float height);
}
