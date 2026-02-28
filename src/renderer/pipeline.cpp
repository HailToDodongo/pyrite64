/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "pipeline.h"
#include "framebuffer.h"
#include "../context.h"

Renderer::Pipeline::Pipeline(const Info &info) {
  SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
  info.shader.setToPipeline(pipelineInfo);
  pipelineInfo.primitive_type = info.prim;

  // Depth
  pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
  pipelineInfo.depth_stencil_state.enable_depth_test = true;
  pipelineInfo.depth_stencil_state.enable_depth_write = true;
  pipelineInfo.target_info.has_depth_stencil_target = true;
  pipelineInfo.target_info.depth_stencil_format = Renderer::getDepthStencilFormat();

  SDL_GPUVertexBufferDescription vertBuffDesc[1];

  vertBuffDesc[0].slot = 0;
  vertBuffDesc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vertBuffDesc[0].instance_step_rate = 0;
  vertBuffDesc[0].pitch = info.vertPitch;

  pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
  pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertBuffDesc;

  int idx = 0;

  std::vector<SDL_GPUVertexAttribute> vertexAttributes{};
  vertexAttributes.resize(info.vertLayout.size());

  for (auto &def : info.vertLayout) {
    vertexAttributes[idx].buffer_slot = 0;
    vertexAttributes[idx].location = idx;
    vertexAttributes[idx].format = def.format;
    vertexAttributes[idx].offset = def.offset;
    ++idx;
  }

  pipelineInfo.vertex_input_state.num_vertex_attributes = vertexAttributes.size();
  pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes.data();

  SDL_GPUColorTargetBlendState blend_state = {};
  blend_state.enable_blend = true;
  blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

  SDL_GPUColorTargetDescription colorTargetDescriptions[2]{
    {
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .blend_state = info.translucent ? blend_state : SDL_GPUColorTargetBlendState{}
    },
    {.format = SDL_GPU_TEXTUREFORMAT_R32_UINT},
  };

  pipelineInfo.target_info.num_color_targets = info.drawsObjID ? 2 : 1;
  pipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;

  pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE; // done in shader
  // pipelineInfo.multisample_state.enable_alpha_to_coverage = true;
  pipeline = SDL_CreateGPUGraphicsPipeline(ctx.gpu, &pipelineInfo);
}

Renderer::Pipeline::~Pipeline() {
  SDL_ReleaseGPUGraphicsPipeline(ctx.gpu, pipeline);
}
