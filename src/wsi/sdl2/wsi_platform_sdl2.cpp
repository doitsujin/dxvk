#include "wsi_platform_sdl2.h"
#include "../util/util_win32_compat.h"

namespace dxvk::wsi {
    WsiLibrary *WsiLibrary::s_instance = nullptr;

    WsiLibrary *WsiLibrary::get() {
      if (s_instance != nullptr)
        return s_instance;

      s_instance = new WsiLibrary();

      // FIXME: When do we free this...?
      s_instance->libsdl = LoadLibraryA( // FIXME: Get soname as string from meson
#if defined(_WIN32)
          "SDL2.dll"
#elif defined(__APPLE__)
          "libSDL2-2.0.0.dylib"
#else
          "libSDL2-2.0.so.0"
#endif
        );
      if (s_instance->libsdl == nullptr)
        throw DxvkError("SDL2 WSI: Failed to load SDL2 DLL.");

      #define SDL_PROC(ret, name, params) \
        s_instance->name = reinterpret_cast<pfn_##name>(GetProcAddress(s_instance->libsdl, #name)); \
        if (s_instance->name == nullptr) \
          throw DxvkError("SDL2 WSI: Failed to load " #name ".");
      #include "wsi_platform_sdl2_funcs.h"

      return s_instance;
    }
}
