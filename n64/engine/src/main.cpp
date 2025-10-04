#include <libdragon.h>
#include <t3d/t3d.h>

#include "../include/scene/sceneManager.h"
#include "vi/swapChain.h"
#include "lib/logger.h"
#include "lib/memory.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "scene/globalState.h"
#include "./audio/audioManagerPrivate.h"

P64::GlobalState P64::state{};

namespace P64::SceneManager {
  void run();
  void unload();
}

[[noreturn]]
int main()
{
	debug_init_isviewer();
	debug_init_usblog();

  asset_init_compression(2);
  // asset_init_compression(3);
  dfs_init(DFS_DEFAULT_LOCATION);

  rdpq_init();
  // rdpq_debug_start();

  P64::AudioManager::init();

  t3d_init({});
  joypad_init();

	P64::Log::info("Starting Game");

  // default VI setup, can be overwritten by the scene load later onŝ
  vi_init();
  vi_set_dedither(false);
  vi_set_aa_mode(vi_aa_mode_t::VI_AA_MODE_RESAMPLE);
  vi_set_interlaced(false);
  vi_set_divot(false);
  vi_set_gamma(vi_gamma_t::VI_GAMMA_DISABLE);

  P64::MatrixManager::reset();
  P64::VI::SwapChain::init();

  P64::SceneManager::load(1);

  for(;;)
  {
    auto heapDiff = P64::Mem::getHeapDiff();
    if(heapDiff != 0) {
      P64::Log::warn("Heap diff: %ld\n", heapDiff);
    }

    P64::SceneManager::run();

	  P64::VI::SwapChain::drain();
	  P64::SceneManager::unload();
	  P64::Mem::freeDepthBuffer();
  }
}
