#include "wsi_platform_glfw.h"
#include "../../util/util_error.h"
#include "../../util/util_string.h"

namespace dxvk::wsi {

  std::vector<const char *> getInstanceExtensions() {
    if (!glfwVulkanSupported())
      throw DxvkError(str::format("GLFW WSI: Vulkan is not supported in any capacity!"));

    uint32_t extensionCount = 0;
    const char** extensionArray = glfwGetRequiredInstanceExtensions(&extensionCount);

    if (extensionCount == 0)
      throw DxvkError(str::format("GLFW WSI: Failed to get required instance extensions"));

    std::vector<const char *> names(extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
      names.push_back(extensionArray[i]);
    }

    return names;
  }

}
