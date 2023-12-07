#include "wsi_platform_glfw.h"
#include "../util/util_win32_compat.h"

namespace dxvk::wsi {
    static WsiLibrary *s_instance = nullptr;
    static int s_refcount = 0;

    WsiLibrary::WsiLibrary(HMODULE dll) {
        libglfw = dll;
    }

    WsiLibrary *WsiLibrary::get() {
      if (s_instance == nullptr)
        throw DxvkError("WSI was not initialized.");
      return s_instance;
    }

    void init() {
      if (s_refcount++ > 0)
        return;

      HMODULE libglfw = LoadLibraryA( // FIXME: Get soname as string from meson
#if defined(_WIN32)
          "glfw.dll"
#elif defined(__APPLE__)
          "libglfw.3.dylib"
#else
          "libglfw.so.3"
#endif
        );
      if (libglfw == nullptr)
        throw DxvkError("GLFW WSI: Failed to load GLFW DLL.");

      s_instance = new WsiLibrary(libglfw);

      #define GLFW_PROC(ret, name, params) \
        s_instance->name = reinterpret_cast<pfn_##name>(GetProcAddress(s_instance->libglfw, #name)); \
        if (s_instance->name == nullptr) \
          throw DxvkError("GLFW WSI: Failed to load " #name ".");
      #include "wsi_platform_glfw_funcs.h"
    }

    void quit() {
      if (s_refcount == 0)
        return;

      s_refcount--;
      FreeLibrary(s_instance->libglfw);
      delete s_instance;
      s_instance = nullptr;
    }
}
