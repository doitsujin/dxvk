#include "wsi_platform_glfw.h"
#include "../util/util_win32_compat.h"

namespace dxvk::wsi {
    WsiLibrary *WsiLibrary::s_instance = nullptr;

    WsiLibrary *WsiLibrary::get() {
      if (s_instance != nullptr)
        return s_instance;

      s_instance = new WsiLibrary();

      // FIXME: When do we free this...?
      s_instance->libglfw = LoadLibraryA( // FIXME: Get soname as string from meson
#if defined(_WIN32)
          "glfw.dll"
#elif defined(__APPLE__)
          "libglfw.3.dylib"
#else
          "libglfw.so.3"
#endif
        );
      if (s_instance->libglfw == nullptr)
        throw DxvkError("GLFW WSI: Failed to load GLFW DLL.");

      #define GLFW_PROC(ret, name, params) \
        s_instance->name = reinterpret_cast<pfn_##name>(GetProcAddress(s_instance->libglfw, #name)); \
        if (s_instance->name == nullptr) \
          throw DxvkError("GLFW WSI: Failed to load " #name ".");
      #include "wsi_platform_glfw_funcs.h"

      return s_instance;
    }
}
