#include "window.h"
#include "../context.h"
#include "../utils/json.h"
#include "../utils/fs.h"
#include <SDL3/SDL.h>
#include "SDL3_image/SDL_image.h"
#include <algorithm>
#include <filesystem>

namespace Editor {

  bool Window::init(const std::string& title) {
    loadState();

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    sdlWindow = SDL_CreateWindow(title.c_str(), state.w, state.h, flags);
    if (!sdlWindow) return false;
    ctx.window = sdlWindow;

    SDL_SetWindowPosition(sdlWindow, state.x, state.y);
    if (state.maximized) {
      SDL_MaximizeWindow(sdlWindow);
    }

    SDL_SetWindowIcon(sdlWindow, IMG_Load("data/img/windowIcon.png"));
    SDL_ShowWindow(sdlWindow);
    
    return true;
  }

  void Window::trackGeometry() {
    if (!sdlWindow) return;
    
    Uint32 flags = SDL_GetWindowFlags(sdlWindow);
    if (!(flags & SDL_WINDOW_MAXIMIZED)) {
        SDL_GetWindowSize(sdlWindow, &state.w, &state.h);
        SDL_GetWindowPosition(sdlWindow, &state.x, &state.y);
    }
    state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
  }

  std::string Window::getConfigPath() {
    auto path = Utils::Proc::getAppDataPath() / "editor.json";
    return path;
  }

  bool Window::loadState() {
    auto path = getConfigPath();
    nlohmann::json json;
    try {
      json = Utils::JSON::loadFile(path);
      if (json.empty()) return false;
    } catch (...) {
      fprintf(stderr, "Warning: corrupt editor.json, using defaults\n");
      return false;
    }
  
    // @TODO: handle actual DPI settings, or have scaling in-editor
    float dpiScale = 1.0f;//SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    state.w = json.value("windowW", (int)(1280 * dpiScale));
    state.h = json.value("windowH", (int)(800 * dpiScale));
    state.x = json.value("windowX", (int)SDL_WINDOWPOS_CENTERED);
    state.y = json.value("windowY", (int)SDL_WINDOWPOS_CENTERED);
    state.maximized = json.value("maximized", false);

    if(state.w <= 0 || state.h <= 0)return false;
    state.w = std::min(state.w, 16384);
    state.h = std::min(state.h, 16384);

    // check that the window is at least partially visible on a connected display
    int numDisplays = 0;
    auto *displays = SDL_GetDisplays(&numDisplays);
    bool posValid = false;
    if(displays) {
      for(int i = 0; i < numDisplays; ++i) {
        SDL_Rect bounds{};
        if(SDL_GetDisplayBounds(displays[i], &bounds)) {
          auto overlapX = std::max(0, std::min(state.x + state.w, bounds.x + bounds.w) - std::max(state.x, bounds.x));
          auto overlapY = std::max(0, std::min(state.y + state.h, bounds.y + bounds.h) - std::max(state.y, bounds.y));
          if(overlapX >= 100 && overlapY >= 100) {
            posValid = true;
            break;
          }
        }
      }
      SDL_free(displays);
    }

    SDL_SetWindowSize(sdlWindow, state.w, state.h);
    if(posValid) {
      SDL_SetWindowPosition(sdlWindow, state.x, state.y);
    } else {
      SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    if(state.maximized) {
      SDL_MaximizeWindow(sdlWindow);
    }
    return true;
  }

  void Window::saveState() const {
    auto path = getConfigPath();
    if (path.empty() || state.w <= 0) return;

    nlohmann::json json;
    json["windowW"] = state.w;
    json["windowH"] = state.h;
    json["windowX"] = state.x;
    json["windowY"] = state.y;
    json["maximized"] = state.maximized;

    Utils::FS::saveTextFile(path, json.dump(2));
  }
}