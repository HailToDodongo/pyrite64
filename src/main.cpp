// Dear ImGui: standalone example application for SDL3 + SDL_GPU
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_sdlgpu3.cpp/.h in their own engine/app.
// - Unlike other backends, the user must call the function ImGui_ImplSDLGPU_PrepareDrawData() BEFORE issuing a SDL_GPURenderPass containing ImGui_ImplSDLGPU_RenderDrawData.
//   Calling the function is MANDATORY, otherwise the ImGui will not upload neither the vertex nor the index buffer for the GPU. See imgui_impl_sdlgpu3.cpp for more info.

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#include "editor/editorScene.h"
#include "renderer/shader.h"
#include "renderer/vertBuffer.h"

// Main code
int main(int, char**)
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
  {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }

  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* window = SDL_CreateWindow("Pyrite64 - Editor", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
  if(window == nullptr)
  {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  // Create GPU Device
  SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,true,nullptr);
  if (gpu_device == nullptr)
  {
      printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
      return -1;
  }

  auto pros = SDL_GetGPUDeviceProperties(gpu_device);
  auto gpuName = SDL_GetStringProperty(pros, SDL_PROP_GPU_DEVICE_NAME_STRING, "");
  printf("Selected GPU: %s\n", gpuName);
  fflush(stdout);

  // Claim window for GPU Device
  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
  {
      printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
      return -1;
  }
  SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
  style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(window);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;                      // Only used in multi-viewports mode.
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;  // Only used in multi-viewports mode.
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  ImGui_ImplSDLGPU3_Init(&init_info);

  style.FontSizeBase = 15.0f;
  ImFont* font = io.Fonts->AddFontFromFileTTF("./data/Altinn-DINExp.ttf");
  IM_ASSERT(font != nullptr);

  Editor::Scene editorScene{};

  std::vector<Renderer::Vertex> vertices{};
  vertices.push_back({0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f});
  vertices.push_back({-0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f});
  vertices.push_back({0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f});

  Renderer::VertBuffer vertBuff{sizeof(vertices), gpu_device};
  vertBuff.setData(vertices);

  // 3D TEST

  Renderer::Shader shader3d{"main3d", gpu_device};

  // load the vertex shader code
  SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
  shader3d.setToPipeline(pipelineInfo);
  pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

  // describe the vertex buffers
  SDL_GPUVertexBufferDescription vertexBufferDesctiptions[1];
  vertexBufferDesctiptions[0].slot = 0;
  vertexBufferDesctiptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vertexBufferDesctiptions[0].instance_step_rate = 0;
  vertexBufferDesctiptions[0].pitch = sizeof(Renderer::Vertex);

  pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
  pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;

  // describe the vertex attribute
  SDL_GPUVertexAttribute vertexAttributes[2];

  // a_position
  vertexAttributes[0].buffer_slot = 0; // fetch data from the buffer at slot 0
  vertexAttributes[0].location = 0; // layout (location = 0) in shader
  vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; //vec3
  vertexAttributes[0].offset = 0; // start from the first byte from current buffer position

  // a_color
  vertexAttributes[1].buffer_slot = 0; // use buffer at slot 0
  vertexAttributes[1].location = 1; // layout (location = 1) in shader
  vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; //vec4
  vertexAttributes[1].offset = sizeof(float) * 3; // 4th float from current buffer position

  pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
  pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

  // describe the color target
  SDL_GPUColorTargetDescription colorTargetDescriptions[1];
  colorTargetDescriptions[0] = {};
  colorTargetDescriptions[0].format = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);

  pipelineInfo.target_info.num_color_targets = 1;
  pipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;

  // create the pipeline
  SDL_GPUGraphicsPipeline* graphicsPipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipelineInfo);
  // we don't need to store the shaders after creating the pipeline
  // SDL_ReleaseGPUGraphicsPipeline(gpu_device, graphicsPipeline);

    // Main loop
    bool done = false;
    while(!done)
    {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
          ImGui_ImplSDL3_ProcessEvent(&event);

          if(event.type == SDL_EVENT_QUIT)done = true;
          if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
            done = true;
          }

          // Check: io.WantCaptureMouse, io.WantCaptureKeyboard
      }

      if(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
      {
          SDL_Delay(10);
          continue;
      }

      ImGui_ImplSDLGPU3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      editorScene.draw();

      // Rendering
      ImGui::Render();
      ImDrawData* draw_data = ImGui::GetDrawData();
      const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

      SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device); // Acquire a GPU command buffer

      SDL_GPUTexture* swapchain_texture;
      SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

      if (swapchain_texture != nullptr && !is_minimized)
      {
          // Setup and start a render pass
          SDL_GPUColorTargetInfo target_info = {};
          target_info.texture = swapchain_texture;
          target_info.clear_color = {0.25f,0.25f,0.25f,0};
          target_info.load_op = SDL_GPU_LOADOP_CLEAR;
          target_info.store_op = SDL_GPU_STOREOP_STORE;
          target_info.mip_level = 0;
          target_info.layer_or_depth_plane = 0;
          target_info.cycle = false;

          {
              ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

              auto copyPass = SDL_BeginGPUCopyPass(command_buffer);
              vertBuff.upload(*copyPass);
              SDL_EndGPUCopyPass(copyPass);
          }

          SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
          SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

          // bind the vertex buffer
          SDL_GPUBufferBinding bufferBindings[1];
          vertBuff.addBinding(bufferBindings[0]);
          SDL_BindGPUVertexBuffers(renderPass, 0, bufferBindings, 1); // bind one buffer starting from slot 0

          SDL_Rect scissorFull{0,0, (int)draw_data->DisplaySize.x, (int)draw_data->DisplaySize.y};
          SDL_Rect scissor3D{0,0, 640, 480};
          //SDL_SetGPUScissor(renderPass, &scissor3D);

          SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

          //SDL_SetGPUScissor(renderPass, &scissorFull);

          // Render ImGui
          ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, renderPass);

          SDL_EndGPURenderPass(renderPass);

      }

      // Submit the command buffer
      SDL_SubmitGPUCommandBuffer(command_buffer);
    }

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    SDL_WaitForGPUIdle(gpu_device);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}