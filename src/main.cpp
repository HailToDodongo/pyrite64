/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "context.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include <stdio.h>
#include <SDL3/SDL.h>
#include <future>

#include <argparse/argparse.hpp>

#include "cli.h"
#include "build/projectBuilder.h"
#include "editor/actions.h"
#include "editor/imgui/theme.h"
#include "editor/pages/editorMain.h"
#include "editor/pages/editorScene.h"
#include "editor/imgui/notification.h"
#include "renderer/scene.h"
#include "renderer/shader.h"
#include "SDL3_image/SDL_image.h"
#include "tiny3d/tools/gltf_importer/src/structs.h"
#include "utils/filePicker.h"
#include "utils/fs.h"
#include "utils/logger.h"
#include "utils/proc.h"

#include <cctype>

Context ctx{};
constinit SDL_GPUSampler *texSamplerRepeat{nullptr};

namespace T3DM
{
  thread_local Config config{};
}

void ImDrawCallback_ImplSDLGPU3_SetSamplerRepeat(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
  auto*           state   = static_cast<ImGui_ImplSDLGPU3_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
  //SDL_GPUSampler* sampler = cmd->UserCallbackData ? static_cast<SDL_GPUSampler*>(cmd->UserCallbackData) : state->SamplerDefault;
  state->SamplerCurrent   = texSamplerRepeat;//sampler;
}

void cli(argparse::ArgumentParser &prog);

// Main code
int main(int argc, char** argv)
{
  fs::current_path(Utils::Proc::getSelfDir());
  ctx.toolchain.scan();

  auto cliRes = CLI::run(argc, argv);
  if (cliRes != CLI::Result::GUI) {
    return (cliRes == CLI::Result::SUCCESS) ? 0 : -1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
  {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_GetTicks();

  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* window = SDL_CreateWindow("Pyrite64 - Editor", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
  ctx.window = window;

  srand(time(NULL) + SDL_GetTicks());

  if(window == nullptr)
  {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_SetWindowIcon(window, IMG_Load("data/img/windowIcon.png"));
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  // Ensure text input events are enabled
  bool useTextInputFallback = false;
  if (!SDL_StartTextInput(window)) {
    useTextInputFallback = true;
    fprintf(stderr, "Warning: SDL_StartTextInput failed: %s\n", SDL_GetError());
  }

  // Create GPU Device
  bool debugMode = false;
  ctx.gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debugMode, nullptr);
  if (ctx.gpu == nullptr)
  {
    printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
    return -1;
  }

  auto pros = SDL_GetGPUDeviceProperties(ctx.gpu);
  auto gpuName = SDL_GetStringProperty(pros, SDL_PROP_GPU_DEVICE_NAME_STRING, "");
  auto gpuDriver = SDL_GetStringProperty(pros, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "");
  printf("Selected GPU: %s | %s\n", gpuName, gpuDriver);
  fflush(stdout);

  // Claim window for GPU Device
  if (!SDL_ClaimWindowForGPUDevice(ctx.gpu, window))
  {
    printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
  if(!SDL_WindowSupportsGPUPresentMode(ctx.gpu, window, presentMode))
  {
    printf("Warning: SDL_GPU_PRESENTMODE_IMMEDIATE not supported, falling back to SDL_GPU_PRESENTMODE_VSYNC\n");
    presentMode = SDL_GPU_PRESENTMODE_VSYNC;
  }

  SDL_SetGPUSwapchainParameters(ctx.gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);

  SDL_GPUSamplerCreateInfo samplerInfo{};
  samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
  samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
  samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  samplerInfo.mip_lod_bias = 0.0f;
  samplerInfo.min_lod = -1000.0f;
  samplerInfo.max_lod = 1000.0f;
  samplerInfo.enable_anisotropy = false;
  samplerInfo.max_anisotropy = 1.0f;
  samplerInfo.enable_compare = false;
  texSamplerRepeat = SDL_CreateGPUSampler(ctx.gpu, &samplerInfo);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr; // TEST
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::applyTheme();
  ImGui::loadFonts(main_scale);

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(window);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = ctx.gpu;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(ctx.gpu, window);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;                      // Only used in multi-viewports mode.
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;  // Only used in multi-viewports mode.
  init_info.PresentMode = presentMode;
  ImGui_ImplSDLGPU3_Init(&init_info);

  {
    Editor::Actions::init();
    Utils::Logger::clear();

    Renderer::Scene scene{};
    ctx.scene = &scene;
    Editor::Main editorMain{ctx.gpu};
    Editor::Scene editorScene{};

    if(!CLI::getProjectPath().empty())
    {
      if(!Editor::Actions::call(Editor::Actions::Type::PROJECT_OPEN, CLI::getProjectPath())) {
        Editor::Noti::add(Editor::Noti::Type::ERROR, "Failed to open project from command line!");
      }
    }

    // Main loop
    bool done = false;
    while(!done) {

      auto frameStart = SDL_GetTicksNS();
      //printf("Frame Start | Time: %.2fms\n", ImGui::GetIO().DeltaTime * 1000.0f);
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
        ImGui_ImplSDL3_ProcessEvent(&event);

        if(event.type == SDL_EVENT_QUIT)done = true;
        if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
          done = true;
        }

        // @TODO: refactor into generic actions with keybinds
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
          if(!ImGui::GetIO().WantTextInput)
          {
            if ((event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_C) {
              Editor::Actions::call(Editor::Actions::Type::COPY);
            }
            if ((event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_V) {
              Editor::Actions::call(Editor::Actions::Type::PASTE);
            }
            if ((event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_S) {
              if (ctx.project) {
                ctx.project->save();
                editorScene.save();
              }
            }
          }

          if (!(event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_F11) {
            Editor::Actions::call(Editor::Actions::Type::PROJECT_BUILD);
          }

          if (!(event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_F12) {
            Editor::Actions::call(Editor::Actions::Type::PROJECT_BUILD, "run");
          }

          if (!(event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_F5) {
            Editor::Actions::call(Editor::Actions::Type::ASSETS_RELOAD);
          }

          if (!(event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_F2)
          {
            presentMode = (presentMode == SDL_GPU_PRESENTMODE_VSYNC) ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
            printf("Switched Present Mode to: %s\n", (presentMode == SDL_GPU_PRESENTMODE_VSYNC) ? "VSync" : "Immediate");
            SDL_SetGPUSwapchainParameters(ctx.gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);
          }

          // Fallback for environments that don't emit SDL_EVENT_TEXT_INPUT (e.g., some WSL setups).
          if (useTextInputFallback && ImGui::GetIO().WantTextInput) {
            SDL_Keymod modstate = (SDL_Keymod)SDL_GetModState();
            const bool hasTextModifiers = (modstate & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
            if (!hasTextModifiers) {
              SDL_Keycode keycode = SDL_GetKeyFromScancode(event.key.scancode, modstate, false);
              if (keycode >= 32 && keycode < 127) {
                ImGui::GetIO().AddInputCharacter((unsigned int)keycode);
              }
            }
          }
        }
        // Check: io.WantCaptureMouse, io.WantCaptureKeyboard
      }

      Utils::FilePicker::poll();
      if (ctx.project) {
        ctx.project->getAssets().pollWatch();
      }

      if(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(10);
        continue;
      }

      ImGui_ImplSDLGPU3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      scene.update();

      if (ctx.project) {
        editorScene.draw();
      } else {
        editorMain.draw();
      }

      Editor::Noti::draw();

      ImGui::Render();
      scene.draw();

      if(presentMode != SDL_GPU_PRESENTMODE_VSYNC)
      {
        uint64_t targetFrameTime = 16'666'666; // ~60 FPS
        auto frameTime = SDL_GetTicksNS() - frameStart;
        if(frameTime < targetFrameTime) {
          SDL_DelayNS(targetFrameTime - frameTime);
        }
      }
    }
  }

  SDL_WaitForGPUIdle(ctx.gpu);

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(ctx.gpu, window);
  SDL_DestroyGPUDevice(ctx.gpu);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
