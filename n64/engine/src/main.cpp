#include <filesystem>
#include <libdragon.h>
#include <t3d/t3d.h>

#include "../include/scene/sceneManager.h"
#include "vi/swapChain.h"
#include "lib/logger.h"
#include "lib/memory.h"
#include "lib/matrixManager.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "scene/globalState.h"
#include "./audio/audioManagerPrivate.h"
#include "assets/assetManager.h"
#include "libdragon/utils.h"
#include "renderer/drawLayer.h"
#include "script/globalScript.h"

P64::GlobalState P64::state{};

namespace P64::SceneManager {
  void run();
  void unload();
}

namespace
{
  struct ProjectConf
  {
    uint32_t sceneIdOnBoot{};
    uint32_t sceneIdOnReset{};
    std::array<uint16_t, 16> autoLoadFonts{}; // index=slot, value=asset-index
  };
  constinit ProjectConf projectConf{};
}

[[noreturn]]
int main()
{
	debug_init_isviewer();
	debug_init_usblog();

  P64::LD::init();

  asset_init_compression(2);
  // asset_init_compression(3);
  dfs_init(DFS_DEFAULT_LOCATION);

  rdpq_init();
  // rdpq_debug_start();

  t3d_init({});
  joypad_init();

  P64::AssetManager::init();
  P64::AudioManager::init();

	P64::Log::info("Starting Game");

  // default VI setup, can be overwritten by the scene load later on
  vi_init();
  vi_set_dedither(false);
  vi_set_aa_mode(VI_AA_MODE_RESAMPLE);
  vi_set_interlaced(false);
  vi_set_divot(false);
  vi_set_gamma(VI_GAMMA_DISABLE);

	{
	  auto tmp = (ProjectConf*)asset_load("rom:/p64/conf", nullptr);
	  projectConf = *tmp;
    free(tmp);
	}

  // auto-load fonts marked as such
  for(uint32_t fontIdx=0; fontIdx < projectConf.autoLoadFonts.size(); fontIdx++) {
    debugf("Auto-load font slot %d: asset index %d\n", fontIdx, projectConf.autoLoadFonts[fontIdx]);
    if(projectConf.autoLoadFonts[fontIdx] < 0xFFFF) {
      auto font = (rdpq_font_t*)P64::AssetManager::getByIndex(projectConf.autoLoadFonts[fontIdx]);
      rdpq_text_register_font(fontIdx, font);
    }
  }

  P64::DrawLayer::reset();
  P64::MatrixManager::reset();
  P64::VI::SwapChain::init();

  P64::GlobalScript::callHooks(P64::GlobalScript::HookType::GAME_INIT);

  P64::Log::info("Reset: %d\n", sys_reset_type());
  P64::SceneManager::load(
    sys_reset_type() == RESET_COLD
      ? projectConf.sceneIdOnBoot
      : projectConf.sceneIdOnReset
  );

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
