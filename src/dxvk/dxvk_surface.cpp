#include "dxvk_surface.h"
#include "dxvk_format.h"

#include "../util/util_math.h"

namespace dxvk {
  
  DxvkSurface::DxvkSurface(
    const Rc<DxvkAdapter>&    adapter,
          HINSTANCE           instance,
          HWND                window)
  : m_adapter         (adapter),
    m_vki             (adapter->vki()),
    m_handle          (createSurface(instance, window)),
    m_surfaceFormats  (getSurfaceFormats()),
    m_presentModes    (getPresentModes()) {
    
  }
  
  
  DxvkSurface::~DxvkSurface() {
    m_vki->vkDestroySurfaceKHR(
      m_vki->instance(), m_handle, nullptr);
  }
  
  
  VkSurfaceCapabilitiesKHR DxvkSurface::getSurfaceCapabilities() const {
    VkSurfaceCapabilitiesKHR surfaceCaps;
    if (m_vki->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          m_adapter->handle(), m_handle, &surfaceCaps) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::getSurfaceCapabilities: Failed to query surface capabilities");
    return surfaceCaps;
  }
  
  
  VkSurfaceFormatKHR DxvkSurface::pickSurfaceFormat(
          uint32_t            preferredCount,
    const VkSurfaceFormatKHR* preferred) const {
    if (preferredCount > 0) {
      // If the implementation allows us to freely choose
      // the format, we'll just use the preferred format.
      if (m_surfaceFormats.size() == 1 && m_surfaceFormats.at(0).format == VK_FORMAT_UNDEFINED)
        return preferred[0];
      
      // If the preferred format is explicitly listed in
      // the array of supported surface formats, use it
      for (uint32_t i = 0; i < preferredCount; i++) {
        for (auto fmt : m_surfaceFormats) {
          if (fmt.format     == preferred[i].format
           && fmt.colorSpace == preferred[i].colorSpace)
            return fmt;
        }
      }

      // If that didn't work, we'll fall back to a format
      // which has similar properties to the preferred one
      DxvkFormatFlags prefFlags = imageFormatInfo(preferred[0].format)->flags;

      for (auto fmt : m_surfaceFormats) {
        auto currFlags = imageFormatInfo(fmt.format)->flags;

        if ((currFlags & DxvkFormatFlag::ColorSpaceSrgb)
         == (prefFlags & DxvkFormatFlag::ColorSpaceSrgb))
          return fmt;
      }
    }
    
    // Otherwise, fall back to the first format
    return m_surfaceFormats.at(0);
  }
  
  
  VkPresentModeKHR DxvkSurface::pickPresentMode(
          uint32_t            preferredCount,
    const VkPresentModeKHR*   preferred) const {
    for (uint32_t i = 0; i < preferredCount; i++) {
      for (auto mode : m_presentModes) {
        if (mode == preferred[i])
          return mode;
      }
    }
    
    // This mode is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
  }
  
  
  uint32_t DxvkSurface::pickImageCount(
    const VkSurfaceCapabilitiesKHR& caps,
          VkPresentModeKHR          mode) const {
    uint32_t count = caps.minImageCount;
    
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR
     || mode == VK_PRESENT_MODE_FIFO_KHR)
      count += 1;
    
    if (count > caps.maxImageCount && caps.maxImageCount != 0)
      count = caps.maxImageCount;
    
    return count;
  }
  
  
  VkExtent2D DxvkSurface::pickImageExtent(
    const VkSurfaceCapabilitiesKHR& caps,
          VkExtent2D                preferred) const {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
      return caps.currentExtent;
    
    VkExtent2D actual;
    actual.width  = clamp(preferred.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    actual.height = clamp(preferred.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return actual;
  }
  
  
  VkSurfaceKHR DxvkSurface::createSurface(HINSTANCE instance, HWND window) {
    VkWin32SurfaceCreateInfoKHR info;
    info.sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.hinstance  = instance;
    info.hwnd       = window;
    
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (m_vki->vkCreateWin32SurfaceKHR(m_vki->instance(), &info, nullptr, &surface) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::createSurface: Failed to create win32 surface");
    
    VkBool32 supportStatus = VK_FALSE;
    
    if (m_vki->vkGetPhysicalDeviceSurfaceSupportKHR(m_adapter->handle(),
          m_adapter->presentQueueFamily(), surface, &supportStatus) != VK_SUCCESS) {
      m_vki->vkDestroySurfaceKHR(m_vki->instance(), surface, nullptr);
      throw DxvkError("DxvkSurface::createSurface: Failed to query surface support");
    }
    
    if (!supportStatus) {
      m_vki->vkDestroySurfaceKHR(m_vki->instance(), surface, nullptr);
      throw DxvkError("DxvkSurface::createSurface: Surface not supported by device");
    }
    
    return surface;
  }
  
  
  std::vector<VkSurfaceFormatKHR> DxvkSurface::getSurfaceFormats() {
    uint32_t numFormats = 0;
    if (m_vki->vkGetPhysicalDeviceSurfaceFormatsKHR(
          m_adapter->handle(), m_handle, &numFormats, nullptr) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::getSurfaceFormats: Failed to query surface formats");
    
    std::vector<VkSurfaceFormatKHR> formats(numFormats);
    if (m_vki->vkGetPhysicalDeviceSurfaceFormatsKHR(
          m_adapter->handle(), m_handle, &numFormats, formats.data()) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::getSurfaceFormats: Failed to query surface formats");
    return formats;
  }
  
  
  std::vector<VkPresentModeKHR> DxvkSurface::getPresentModes() {
    uint32_t numModes = 0;
    if (m_vki->vkGetPhysicalDeviceSurfacePresentModesKHR(
          m_adapter->handle(), m_handle, &numModes, nullptr) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::getPresentModes: Failed to query present modes");
    
    std::vector<VkPresentModeKHR> modes(numModes);
    if (m_vki->vkGetPhysicalDeviceSurfacePresentModesKHR(
          m_adapter->handle(), m_handle, &numModes, modes.data()) != VK_SUCCESS)
      throw DxvkError("DxvkSurface::getPresentModes: Failed to query present modes");
    return modes;
  }
  
}