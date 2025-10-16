/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene.h"
#include <SDL3/SDL.h>

#include "imgui.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "../context.h"

#include "./texture.h"
#include "./vertBuffer.h"
#include "framebuffer.h"
#include "shader.h"

namespace
{
  SDL_GPUGraphicsPipeline* graphicsPipeline{nullptr};
  Renderer::Shader *shader3d{nullptr};
}

Renderer::Scene::Scene()
{
  shader3d = new Shader{"main3d", ctx.gpu};

  // load the vertex shader code
  SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
  shader3d->setToPipeline(pipelineInfo);
  pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

  // Depth
  pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  pipelineInfo.depth_stencil_state.enable_depth_test = true;
  pipelineInfo.depth_stencil_state.enable_depth_write = true;
  pipelineInfo.target_info.has_depth_stencil_target = true;
  pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

  // describe the vertex buffers
  SDL_GPUVertexBufferDescription vertBuffDesc[1];
  vertBuffDesc[0].slot = 0;
  vertBuffDesc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vertBuffDesc[0].instance_step_rate = 0;
  vertBuffDesc[0].pitch = sizeof(Renderer::Vertex);

  pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
  pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertBuffDesc;

  // describe the vertex attribute
  std::array<SDL_GPUVertexAttribute, 4> vertexAttributes{};
  int idx = 0;

  // Position
  vertexAttributes[idx].buffer_slot = 0; // fetch data from the buffer at slot 0
  vertexAttributes[idx].location = idx; // layout (location = 0) in shader
  vertexAttributes[idx].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  vertexAttributes[idx].offset = offsetof(Renderer::Vertex, pos);
  ++idx;

  // Normal
  vertexAttributes[idx].buffer_slot = 0;
  vertexAttributes[idx].location = idx;
  vertexAttributes[idx].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  vertexAttributes[idx].offset = offsetof(Renderer::Vertex, norm);
  ++idx;

  // Color
  vertexAttributes[idx].buffer_slot = 0;
  vertexAttributes[idx].location = idx;
  vertexAttributes[idx].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  vertexAttributes[idx].offset = offsetof(Renderer::Vertex, color);
  ++idx;

  // UV
  vertexAttributes[idx].buffer_slot = 0;
  vertexAttributes[idx].location = idx;
  vertexAttributes[idx].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  vertexAttributes[idx].offset = offsetof(Renderer::Vertex, uv);
  ++idx;

  pipelineInfo.vertex_input_state.num_vertex_attributes = vertexAttributes.size();
  pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes.data();

  // describe the color target
  SDL_GPUColorTargetDescription colorTargetDescriptions[1] {
    {.format = SDL_GetGPUSwapchainTextureFormat(ctx.gpu, ctx.window)}
  };

  pipelineInfo.target_info.num_color_targets = 1;
  pipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;

  graphicsPipeline = SDL_CreateGPUGraphicsPipeline(ctx.gpu, &pipelineInfo);
}

Renderer::Scene::~Scene() {
  SDL_ReleaseGPUGraphicsPipeline(ctx.gpu, graphicsPipeline);
  //SDL_ReleaseGPUTexture(ctx.gpu, fb3D);
}

void Renderer::Scene::update()
{
}

void Renderer::Scene::draw()
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(ctx.gpu); // Acquire a GPU command buffer

  SDL_GPUTexture* swapchain_texture;
  SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, ctx.window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

  if(swapchain_texture == nullptr || is_minimized) {
    SDL_SubmitGPUCommandBuffer(command_buffer);
    return;
  }

  SDL_GPUColorTargetInfo targetInfo2D = {};
  targetInfo2D.texture = swapchain_texture;
  targetInfo2D.clear_color = {0.12f, 0.12f, 0.12f, 1};
  targetInfo2D.load_op = SDL_GPU_LOADOP_CLEAR;
  targetInfo2D.store_op = SDL_GPU_STOREOP_STORE;
  targetInfo2D.mip_level = 0;
  targetInfo2D.layer_or_depth_plane = 0;
  targetInfo2D.cycle = false;

  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

  auto copyPass = SDL_BeginGPUCopyPass(command_buffer);
  for (const auto &passCb : copyPasses) {
    passCb.second(command_buffer, copyPass);
  }
  for (const auto &passCb : copyPassesOneTime) {
    passCb(command_buffer, copyPass);
  }
  copyPassesOneTime.clear();
  SDL_EndGPUCopyPass(copyPass);

  if (ctx.project)
  {
    for (const auto &passCb : renderPasses) {
      passCb.second(command_buffer, graphicsPipeline);
    }
  }

  // Render ImGui
  SDL_GPURenderPass* renderPass2D = SDL_BeginGPURenderPass(command_buffer, &targetInfo2D, 1, nullptr);
  ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, renderPass2D);
  SDL_EndGPURenderPass(renderPass2D);
  // Submit the command buffer
  SDL_SubmitGPUCommandBuffer(command_buffer);
}
