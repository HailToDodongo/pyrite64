/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "vi/swapChain.h"

#include "vi.h"
#include "lib/fifo.h"
#include "lib/logger.h"
#include "lib/ringBuffer.h"

namespace {
  constexpr uint32_t FB_COUNT = 3;
  volatile uint8_t fbIdxVI = 0;

  std::array<uint8_t, FB_COUNT> fbState{}; // current render-pass index
  P64::Lib::FIFO<uint8_t, 0xFF, FB_COUNT> fbIdxForVI{};
  volatile uint32_t fbFreeCount = 0; // amount of 'fbState' at zero, used for a faster loop

  // prevent a new frame from being started, this is done to avoid multiple passes in parallel.
  // At least up until the VI takes over. Otherwise, it runs risk of doing 2 RDP passes in parallel
  // leading to random corruptions
  volatile uint8_t blockNewFrame = false;
  constinit surface_t *frameBuffers = nullptr;

  constinit uint64_t lastTicks{};
  constinit P64::RingBuffer<float, 6> lastDeltaTimes{};
  constinit float avgDeltaTime{};
  constinit float avgFps{};
  constinit float refreshRate{};
  constinit float refreshRateRound{};
  constinit bool vblankEnabled{false};

  P64::VI::SwapChain::RenderPassDrawTask drawTask{nullptr};
  uint32_t frameSkip = 0;
  uint32_t frameIdx = 0;

  void onVIFrameReady([[maybe_unused]] void *userData)
  {
    if(++frameIdx <= frameSkip)return;
    disable_interrupts();
    auto nextFbIdx = fbIdxForVI.pop();

    if(nextFbIdx != 0xFF) {
      vi_write_begin();
        vi_show(&frameBuffers[nextFbIdx]);
      vi_write_end();

      ++fbState[nextFbIdx];
      fbState[fbIdxVI] = 0;
      fbFreeCount += 1;
      fbIdxVI = nextFbIdx;
    }
    enable_interrupts();
    frameIdx = 0;
  }

  /**
   * Gets called by an async render-pass, this will mark it as ready for the next pass.
   * This is usually called from within an interrupt, so we have to defer the transition to the next pass.
   * @param fbIndex
   */
  void renderPassDone(uint32_t fbIndex)
  {
    disable_interrupts();
    ++fbState[fbIndex];
    fbIdxForVI.push(fbIndex);
    blockNewFrame = false;
    enable_interrupts();
  }
}

void P64::VI::SwapChain::init()
{
  frameBuffers = nullptr;
  blockNewFrame = false;

  fbState.fill({0xFF-1}); // block all buffers...
  fbState[1] = 0; // ...except second, picked up by first render-pass
  fbFreeCount = 1;
  // to get started, pretend the VI already has 1 frame rendering
  // this will start the logic of VI chasing finished buffers + freeing drawn ones
  fbIdxVI = FB_COUNT-1;
  fbIdxForVI.fill(0xFF); // clear FIFO...
  fbIdxForVI.push(0); // ...and make VI render the first buffer

  lastTicks = get_ticks() - TICKS_FROM_MS(16);
  avgDeltaTime = 1.0f / 60.0f;
  lastDeltaTimes.fill(avgDeltaTime);

  refreshRate = calcRefreshRate();
  refreshRateRound = roundf(refreshRate);

  disable_interrupts();
    vi_install_vblank_handler(onVIFrameReady, nullptr);
  enable_interrupts();

  rspq_wait();
}

void P64::VI::SwapChain::setVBlank(bool enabled)
{
  if(vblankEnabled != enabled) {
    vblankEnabled = enabled;
    vi_blank(vblankEnabled);
  }
}

float P64::VI::SwapChain::getDeltaTime()
{
  return avgDeltaTime;
}

float P64::VI::SwapChain::getFPS()
{
  return avgFps;
}

void P64::VI::SwapChain::nextFrame() {
  for (uint32_t __t = TICKS_READ() + TICKS_FROM_MS(200);; __rsp_check_assert(__FILE__, __LINE__, __func__))
  {
    if(fbFreeCount && !blockNewFrame)break;
    if(!TICKS_BEFORE(TICKS_READ(), __t)) {
      //rsp_crashf("wait loop timed out (%d ms)", 200);
      Log::error("RSP time-out, force new buffer");
      fbFreeCount = 1;
      blockNewFrame = false;
    }
  }

  uint32_t freeIdx = 0;
  while(fbState[freeIdx])++freeIdx;

  uint64_t newTicks = get_ticks();
  uint64_t ticksDiff = newTicks - lastTicks;

  float newDelta = (float)((double)TICKS_TO_US(ticksDiff) * (1.0/1e6));
  if(newDelta > (1.0f / 20.0f)) { // @TODO: somtimes this gets huge values in the thousands
    //debugf("DELTA-TIME: %.4f (%lld - %lld)\n", newDelta, lastTicks, newTicks);
    newDelta = (1.0f / 20.0f);
  }

  lastTicks = newTicks;
  lastDeltaTimes.push(newDelta);
  avgDeltaTime = lastDeltaTimes.average();

  avgFps = (1.0f / avgDeltaTime) / refreshRate * refreshRateRound;
  avgFps = fminf(avgFps, refreshRateRound);

  disable_interrupts();
  fbFreeCount -= 1;
  blockNewFrame = true;
  enable_interrupts();

  drawTask(&frameBuffers[freeIdx], freeIdx, renderPassDone);
}

void P64::VI::SwapChain::drain() {
  rspq_wait();
  RSP_WAIT_LOOP(200) {
    // if only one buffer is not free (must be VI), we are done
    if(fbFreeCount == (FB_COUNT - 1))break;
  }
  blockNewFrame = false;
}

void P64::VI::SwapChain::setFrameSkip(uint32_t skip) {
  frameSkip = skip;
}

void P64::VI::SwapChain::setDrawPass(SwapChain::RenderPassDrawTask task) {
  drawTask = task;
}

void P64::VI::SwapChain::start() {
  if(vblankEnabled)return;

  vi_write_begin();
    vi_show(&frameBuffers[fbIdxVI]);
  vi_write_end();
}

void P64::VI::SwapChain::setFrameBuffers(surface_t buffers[3]) {
  frameBuffers = buffers;
}

surface_t *P64::VI::SwapChain::getFrameBuffer(uint32_t idx) {
  return &frameBuffers[idx];
}
