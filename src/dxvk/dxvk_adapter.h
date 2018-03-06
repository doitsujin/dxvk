#pragma once

#include "./vulkan/dxvk_vulkan_extensions.h"

#include "dxvk_include.h"

namespace dxvk {
  
  class DxvkDevice;
  class DxvkInstance;
  class DxvkSurface;
  
  /**
   * \brief GPU vendors
   * Based on PCIe IDs.
   */
  enum class DxvkGpuVendor : uint16_t {
    Amd    = 0x1002,
    Nvidia = 0x10de,
    Intel  = 0x8086,
  };
  
  /**
   * \brief DXVK adapter
   * 
   * Corresponds to a physical device in Vulkan. Provides
   * all kinds of information about the device itself and
   * the supported feature set.
   */
  class DxvkAdapter : public RcObject {
    
  public:
    
    DxvkAdapter(
      const Rc<DxvkInstance>&   instance,
            VkPhysicalDevice    handle);
    ~DxvkAdapter();
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const {
      return m_vki;
    }
    
    /**
     * \brief Physical device handle
     * \returns The adapter handle
     */
    VkPhysicalDevice handle() const {
      return m_handle;
    }
    
    /**
     * \brief Physical device properties
     * 
     * Retrieves information about the device itself.
     * \returns Physical device properties
     */
    VkPhysicalDeviceProperties deviceProperties() const;
    
    /**
     * \brief Memory properties
     * 
     * Queries the memory types and memory heaps of
     * the device. This is useful for memory allocators.
     * \returns Device memory properties
     */
    VkPhysicalDeviceMemoryProperties memoryProperties() const;
    
    /**
     * \brief Supportred device features
     * 
     * Queries the supported device features.
     * \returns Device features
     */
    VkPhysicalDeviceFeatures features() const;
    
    /**
     * \brief Queries format support
     * 
     * \param [in] format The format to query
     * \returns Format support info
     */
    VkFormatProperties formatProperties(
      VkFormat format) const;
    
    /**
     * \brief Queries image format support
     * 
     * \param [in] format Format to query
     * \param [in] type Image type
     * \param [in] tiling Image tiling
     * \param [in] usage Image usage flags
     * \param [in] flags Image create flags
     * \param [out] properties Format properties
     * \returns \c VK_SUCCESS or \c VK_ERROR_FORMAT_NOT_SUPPORTED
     */
    VkResult imageFormatProperties(
      VkFormat                  format,
      VkImageType               type,
      VkImageTiling             tiling,
      VkImageUsageFlags         usage,
      VkImageCreateFlags        flags,
      VkImageFormatProperties&  properties) const;
    
    /**
     * \brief Graphics queue family index
     * \returns Graphics queue family index
     */
    uint32_t graphicsQueueFamily() const;
    
    /**
     * \brief Presentation queue family index
     * \returns Presentation queue family index
     */
    uint32_t presentQueueFamily() const;
    
    /**
     * \brief Tests whether all required features are supported
     * 
     * \param [in] features Required device features
     * \returns \c true if all features are supported
     */
    bool checkFeatureSupport(
      const VkPhysicalDeviceFeatures& required) const;
    
    /**
     * \brief Creates a DXVK device
     * 
     * Creates a logical device for this adapter.
     * \param [in] enabledFeatures Device features
     * \returns Device handle
     */
    Rc<DxvkDevice> createDevice(
      const VkPhysicalDeviceFeatures& enabledFeatures);
    
    /**
     * \brief Creates a surface
     * 
     * \param [in] instance Module instance
     * \param [in] window Application window
     * \returns Surface handle
     */
    Rc<DxvkSurface> createSurface(
      HINSTANCE instance,
      HWND      window);
    
    /**
     * \brief Logs DXVK adapter info
     * 
     * May be useful for bug reports
     * and general troubleshooting.
     */
    void logAdapterInfo() const;
    
  private:
    
    Rc<DxvkInstance>    m_instance;
    Rc<vk::InstanceFn>  m_vki;
    VkPhysicalDevice    m_handle;
    
    std::vector<VkQueueFamilyProperties> m_queueFamilies;
    
    static void logNameList(const vk::NameList& names);
    
  };
  
}