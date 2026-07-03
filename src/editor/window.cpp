#include "window.h"
#include "../context.h"
#include "../utils/json.h"
#include "../utils/fs.h"
#include <SDL3/SDL.h>
#include "SDL3_image/SDL_image.h"
#include <algorithm>
#include <filesystem>

#include "imgui/theme.h"

namespace Editor {
  constexpr Sint64 default_width = 1280;
  constexpr Sint64 default_height = 720;

  static bool isWayland() {
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver && std::string(driver) == "wayland";
  }

  Window::~Window()
  {
    if(icon) {
      SDL_DestroySurface(icon);
    }
  }

  bool Window::init(const std::string& title) {
    loadState();

    if (isWayland()) {
      // Session ID does not target a specific window, its a top level property, 
      // individual windows get their own identifiers.
      SDL_SetStringProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_SESSION_ID_STRING, state.sessionID.c_str());
    }
    
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
    
    if (isWayland()) {
      SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WINDOW_ID_STRING, "main_window");
      if (state.sessionID.empty()) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, default_width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, default_height);
      }
    } else {
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, state.w);
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, state.h);
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, state.x);
      SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, state.y);
      SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, state.maximized);
    }

    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!sdlWindow) return false;
    ctx.window = sdlWindow;

    icon = IMG_Load("data/img/windowIcon.png");
    SDL_SetWindowIcon(sdlWindow, icon);
    SDL_ShowWindow(sdlWindow);
    
    return true;
  }

  void Window::trackGeometry() {
    if (!sdlWindow) return;
    Uint32 flags = SDL_GetWindowFlags(sdlWindow);
    
    state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    if (isWayland()) return;

    if (!(flags & SDL_WINDOW_MAXIMIZED)) {
        SDL_GetWindowSize(sdlWindow, &state.w, &state.h);
        SDL_GetWindowPosition(sdlWindow, &state.x, &state.y);
    }
  }

  std::string Window::getConfigPath() {
    auto path = Utils::Proc::getAppDataPath() / "editor.json";
    return path.string();
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
    
    int zoomLevel = json.value("zoomLevel", -1);
    if(zoomLevel >= 0) {
      ImGui::Theme::setZoomLevel(zoomLevel);
    }
    state.maximized = json.value("maximized", false); 

    if (isWayland()) {
      state.sessionID = json.value("sessionID", "");
      return true;
    }

    state.w = json.value("windowW", default_width);
    state.h = json.value("windowH", default_height);
    state.x = json.value("windowX", (int)SDL_WINDOWPOS_CENTERED);
    state.y = json.value("windowY", (int)SDL_WINDOWPOS_CENTERED);

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
    if (isWayland()) {
      json["sessionID"] = SDL_GetStringProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_SESSION_ID_STRING, "");
    }
    else {
      json["windowX"] = state.x;
      json["windowY"] = state.y;
      // While these are accesible under wayland, without position they are functionally useless
      json["windowW"] = state.w;
      json["windowH"] = state.h;
    }
    json["maximized"] = state.maximized;
    json["zoomLevel"] = ImGui::Theme::getZoomLevel();

    Utils::FS::saveTextFile(path, json.dump(2));
  }
}
