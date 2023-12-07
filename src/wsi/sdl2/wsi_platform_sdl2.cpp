#include "wsi_platform_sdl2.h"
#include "../util/util_win32_compat.h"

namespace dxvk::wsi {
    static WsiLibrary *s_instance = nullptr;
    static int s_refcount = 0;

    WsiLibrary::WsiLibrary(HMODULE dll) {
        libsdl = dll;
    }

    WsiLibrary *WsiLibrary::get() {
      if (s_instance == nullptr)
        throw DxvkError("WSI was not initialized.");
      return s_instance;
    }

    void init() {
      if (s_refcount++ > 0)
        return;

      HMODULE libsdl = LoadLibraryA( // FIXME: Get soname as string from meson
#if defined(_WIN32)
          "SDL2.dll"
#elif defined(__APPLE__)
          "libSDL2-2.0.0.dylib"
#else
          "libSDL2-2.0.so.0"
#endif
        );
      if (libsdl == nullptr)
        throw DxvkError("SDL2 WSI: Failed to load SDL2 DLL.");

      s_instance = new WsiLibrary(libsdl);

      #define SDL_PROC(ret, name, params) \
        s_instance->name = reinterpret_cast<pfn_##name>(GetProcAddress(s_instance->libsdl, #name)); \
        if (s_instance->name == nullptr) \
          throw DxvkError("SDL2 WSI: Failed to load " #name ".");
      #include "wsi_platform_sdl2_funcs.h"
    }

    void quit() {
      if (s_refcount == 0)
        return;

      s_refcount--;
      FreeLibrary(s_instance->libsdl);
      delete s_instance;
      s_instance = nullptr;
    }
}
