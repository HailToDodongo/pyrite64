/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  class Scene;
  class Camera;

  class RenderPipeline
  {
    protected:
      Scene &scene;

      void setupLayer();

      surface_t *surfColor{};
      surface_t *surfDepth{};

    public:
      explicit RenderPipeline(Scene &sc) : scene{sc} {}

      [[nodiscard]] surface_t* getCurrColorSurf() const { return surfColor; }
      [[nodiscard]] surface_t* getCurrDepthSurf() const { return surfDepth; }

      virtual ~RenderPipeline() = default;
      virtual void init() = 0;
      virtual void beginFrame() = 0;
      virtual void endFrame() = 0;

      // Frame phases, called by the scene in this order:
      //   beginFrame -> [beginCamera -> objects -> endCamera] per camera -> endFrame
      // The camera hooks run while the camera is attached. By default a camera's 3D and particle
      // layers are drawn in 'endCamera', pipelines that need them after their own passes
      // (BigTex) override both camera hooks and draw the layers in 'endFrame'.
      virtual void beginCamera(Camera&) {}
      virtual void endCamera(Camera&);
  };

  class RenderPipelineDefault final : public RenderPipeline
  {
    private:
      surface_t surfFbColor[3]{};

    public:
      using RenderPipeline::RenderPipeline;
      ~RenderPipelineDefault() override;

      void init() override;
      void beginFrame() override;
      void endFrame() override;
  };
}
