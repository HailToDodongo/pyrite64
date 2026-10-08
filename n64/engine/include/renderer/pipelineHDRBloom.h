/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "pipeline.h"
#include "./hdr/postProcess.h"

namespace P64
{
  struct HDRBloomConf {
    int blurSteps{}; // how often to blur the low-res image
    float blurBrightness{}; // brightness of the blur aka bloom
    float hdrFactor{}; // HDR exposure factor, 1.0 to get standard color range
    float bloomThreshold{}; // threshold to ignore pixels before blurring
    bool scalingUseRDP{}; // if true, use RDP for initial downscaling
  };

  class RenderPipelineHDRBloom final : public RenderPipeline
  {
    private:
      // Output
      constexpr static uint32_t BUFF_COUNT = 3;
      surface_t surfFbColor[BUFF_COUNT]{};
      Renderer::HDR::PostProcess postProc[BUFF_COUNT]{};
      uint32_t frameIdx{};

    public:
      using RenderPipeline::RenderPipeline;
      ~RenderPipelineHDRBloom() override;

      void init() override;
      void beginFrame() override;
      void endFrame() override;
  };
}