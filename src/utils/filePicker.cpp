/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "filePicker.h"

#include <atomic>
#include <mutex>
#include <SDL3/SDL.h>

#include "../context.h"

namespace
{
  constinit bool isPickerOpen{false};
  std::mutex mtxResult{};
  constinit std::atomic_bool hasResult{false};
  constinit std::string result{};

  constinit std::vector<SDL_DialogFileFilter> filefilter{};

  std::function<void(const std::string&path)> resultUserCb;

  void cbResult(void *userdata, const char * const *filelist, int filter) {
    std::lock_guard lock{mtxResult};
    result = (filelist && filelist[0]) ? filelist[0] : "";
    hasResult = true;
  }
}

bool Utils::FilePicker::open(std::function<void(const std::string&path)> cb, const Options &options) {
  if (isPickerOpen) return false;

  resultUserCb = cb;
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, ctx.window);
  //SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, default_location);
  SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
  if(!options.title.empty()) {
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, options.title.c_str());
  }

  filefilter.clear();
  for (const auto &f : options.customFilters) {
    filefilter.push_back({f.name, f.pattern});
  }

  if(!filefilter.empty()) {
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, filefilter.data());
    SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, filefilter.size());
  }

  SDL_ShowFileDialogWithProperties(
    options.isDirectory ? SDL_FILEDIALOG_OPENFOLDER : SDL_FILEDIALOG_OPENFILE,
    cbResult, nullptr, props
  );
  SDL_DestroyProperties(props);

  isPickerOpen = true;
  return true;
}

void Utils::FilePicker::poll()
{
  if (hasResult) {
    mtxResult.lock();
    auto resLocal = result;
    mtxResult.unlock();

    if (!resLocal.empty()) {
      resultUserCb(resLocal);
    }

    isPickerOpen = false;
    hasResult = false;
  }
}
