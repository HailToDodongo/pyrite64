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
}

Renderer::Scene::Scene()
{
  shaderN64 = std::make_unique<Shader>(ctx.gpu, Shader::Config{
    .name = "n64",
    .vertUboCount = 2,
    .fragUboCount = 1,
    .vertTexCount = 2,
    .fragTexCount = 2,
  });
  shaderLines = std::make_unique<Shader>(ctx.gpu, Shader::Config{
    .name = "lines",
    .vertUboCount = 2,
    .fragUboCount = 0,
    .vertTexCount = 0,
    .fragTexCount = 0,
  });

  pipelineN64 = std::make_unique<Pipeline>(Pipeline::Info{
    .shader = *shaderN64,
    .prim = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .useDepth = true,
    .drawsObjID = true,
    .vertPitch = sizeof(Vertex),
    .vertLayout = {
      {SDL_GPU_VERTEXELEMENTFORMAT_SHORT4     , offsetof(Renderer::Vertex, pos)},
      {SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Renderer::Vertex, color)},
      {SDL_GPU_VERTEXELEMENTFORMAT_SHORT2    ,  offsetof(Renderer::Vertex, uv)},
    }
  });

  pipelineLines = std::make_unique<Pipeline>(Pipeline::Info{
    .shader = *shaderLines,
    .prim = SDL_GPU_PRIMITIVETYPE_LINELIST,
    .useDepth = true,
    .drawsObjID = false,
    .vertPitch = sizeof(LineVertex),
    .vertLayout = {
      {SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3     , offsetof(Renderer::LineVertex, pos)},
      {SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Renderer::LineVertex, color)},
    }
  });
}

Renderer::Scene::~Scene() {
  //SDL_ReleaseGPUTexture(ctx.gpu, fb3D);
}

void Renderer::Scene::update()
{
}

void Renderer::Scene::draw()
{
  const auto drawData = ImGui::GetDrawData();
  const bool isMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(ctx.gpu); // Acquire a GPU command buffer

  SDL_GPUTexture* swapTex{};
  SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, ctx.window, &swapTex, nullptr, nullptr); // Acquire a swapchain texture

  if(swapTex == nullptr || isMinimized) {
    SDL_SubmitGPUCommandBuffer(command_buffer);
    return;
  }

  SDL_GPUColorTargetInfo targetInfo2D = {};
  targetInfo2D.texture = swapTex;
  targetInfo2D.clear_color = {0.12f, 0.12f, 0.12f, 1};
  targetInfo2D.load_op = SDL_GPU_LOADOP_CLEAR;
  targetInfo2D.store_op = SDL_GPU_STOREOP_STORE;
  targetInfo2D.mip_level = 0;
  targetInfo2D.layer_or_depth_plane = 0;
  targetInfo2D.cycle = false;

  ImGui_ImplSDLGPU3_PrepareDrawData(drawData, command_buffer);

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
      passCb.second(command_buffer, *this);
    }
  }

  // Render ImGui
  SDL_GPURenderPass* renderPass2D = SDL_BeginGPURenderPass(command_buffer, &targetInfo2D, 1, nullptr);
  ImGui_ImplSDLGPU3_RenderDrawData(drawData, command_buffer, renderPass2D);
  SDL_EndGPURenderPass(renderPass2D);
  // Submit the command buffer
  SDL_SubmitGPUCommandBuffer(command_buffer);

  if (ctx.project)
  {
    for (const auto &cb : postRenderCallback) {
      cb.second(*this);
    }
  }
}
