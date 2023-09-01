#include "../dxvk_platform_exts.h"

#include "../../vulkan/vulkan_loader.h"
#include <GLFW/glfw3.h>

namespace dxvk {

  DxvkPlatformExts DxvkPlatformExts::s_instance;

  std::string_view DxvkPlatformExts::getName() {
    return "GLFW WSI";
  }

  DxvkNameSet DxvkPlatformExts::getInstanceExtensions() {
    if (!glfwVulkanSupported()) 
      throw DxvkError(str::format("GLFW WSI: Vulkan is not supported in any capacity!"));

    uint32_t extensionCount = 0;
    const char** extensionArray = glfwGetRequiredInstanceExtensions(&extensionCount);

    if (extensionCount == 0)
      throw DxvkError(str::format("GLFW WSI: Failed to get required instance extensions"));
        
    DxvkNameSet names;
    for (uint32_t i = 0; i < extensionCount; ++i) { 
      names.add(extensionArray[i]);
    }

    return names;
  }


  DxvkNameSet DxvkPlatformExts::getDeviceExtensions(
          uint32_t      adapterId) {
    return DxvkNameSet();
  }


  void DxvkPlatformExts::initInstanceExtensions() {
    //Nothing needs to be done here on GLFW
  }


  void DxvkPlatformExts::initDeviceExtensions(
    const DxvkInstance* instance) {
    //Nothing needs to be done here on GLFW
  }

}