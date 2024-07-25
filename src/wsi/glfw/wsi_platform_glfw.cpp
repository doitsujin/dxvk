#if defined(DXVK_WSI_GLFW)

#include "wsi_platform_glfw.h"
#include "../../util/util_error.h"
#include "../../util/util_string.h"
#include "../../util/util_win32_compat.h"

namespace dxvk::wsi {

  GlfwWsiDriver::GlfwWsiDriver() {
    libglfw = LoadLibraryA( // FIXME: Get soname as string from meson
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

    #define GLFW_PROC(ret, name, params) \
      name = reinterpret_cast<pfn_##name>(GetProcAddress(libglfw, #name)); \
      if (name == nullptr) { \
        FreeLibrary(libglfw); \
        libglfw = nullptr; \
        throw DxvkError("GLFW WSI: Failed to load " #name "."); \
      }
    #include "wsi_platform_glfw_funcs.h"
  }

  GlfwWsiDriver::~GlfwWsiDriver() {
    FreeLibrary(libglfw);
  }

  std::vector<const char *> GlfwWsiDriver::getInstanceExtensions() {
    if (!glfwVulkanSupported())
      throw DxvkError(str::format("GLFW WSI: Vulkan is not supported in any capacity!"));

    uint32_t extensionCount = 0;
    const char** extensionArray = glfwGetRequiredInstanceExtensions(&extensionCount);

    if (extensionCount == 0)
      throw DxvkError(str::format("GLFW WSI: Failed to get required instance extensions"));

    std::vector<const char *> names;
    for (uint32_t i = 0; i < extensionCount; ++i) {
      names.push_back(extensionArray[i]);
    }

    return names;
  }

  static bool createGlfwWsiDriver(WsiDriver **driver) {
    try {
      *driver = new GlfwWsiDriver();
    } catch (const DxvkError& e) {
      return false;
    }
    return true;
  }

  WsiBootstrap GlfwWSI = {
    "GLFW",
    createGlfwWsiDriver
  };

}

#endif
