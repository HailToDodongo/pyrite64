/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <tsq/tsq.h>
#include <tsq/sfx3d.h>
#include <tsq/streamPlayer.h>
#include "assets/assetManager.h"
#include "collision/gfxScale.h"

/**
 * @TODO: remove this?
 */
namespace P64::Audio
{
  extern TSQ::LivePlayer *sfx;

  /** The engine-owned sound engine, valid for the entire game runtime. */
  inline TSQ::SoundEngine &engine() { return *TSQ::SoundEngine::instance(); }

  /** Typed access to a `.tsw` sample asset (loads it if needed). */
  inline TSQ::SfxSample &sample(uint32_t assetId) {
    return *(TSQ::SfxSample*)AssetManager::getByIndex(assetId);
  }

  /** Typed access to a `.tsf` sound-font asset (loads it if needed). */
  inline TSQ::SoundBank *bank(uint32_t assetId) {
    return (TSQ::SoundBank*)AssetManager::getByIndex(assetId);
  }

  /**
   * DFS path of a path-only audio asset (`.tsq` sequences, `.wav64` streams),
   * ready to be passed to SoundEngine::createPlayer or StreamPlayer::open.
   */
  inline const char* path(uint32_t assetId) {
    return AssetManager::getPathByIndex(assetId);
  }

  /**
   * Converts a world position into the meters the 3D audio works in
   * (see the scene's 'Units per Meter' setting).
   * All positions given to the listener or a 3D sound must go through this.
   */
  inline fm_vec3_t toMeters(const fm_vec3_t &pos) {
    return pos * Coll::getInvGfxScale();
  }
}
