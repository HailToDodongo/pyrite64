#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace Editor {
  struct WindowState {
    int x{SDL_WINDOWPOS_CENTERED};
    int y{SDL_WINDOWPOS_CENTERED};
    int w{1280};
    int h{800};
    bool maximized{false};
  };

  class Window {
    public:
      Window() = default;

      bool init(const std::string& title);
      void trackGeometry();
      void saveState() const;
      bool loadState();

    private:
      SDL_Window* sdlWindow{nullptr};
      WindowState state{};
      
      static std::string getConfigPath();
  };
}