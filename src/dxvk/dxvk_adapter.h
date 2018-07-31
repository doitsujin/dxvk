#pragma once

#include "dxvk_device_info.h"
#include "dxvk_extensions.h"
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
     * \brief Vulkan instance
     * \returns Vulkan instance
     */
    Rc<DxvkInstance> instance() const;
    
    /**
     * \brief Physical device properties
     * 
     * Returns a read-only reference to the core
     * properties of the Vulkan physical device.
     * \returns Physical device core properties
     */
    const VkPhysicalDeviceProperties& deviceProperties() const {
      return m_deviceInfo.core.properties;
    }

    /**
     * \brief Device info
     * 
     * Returns a read-only reference to the full
     * device info structure, including extended
     * properties.
     * \returns Device info struct
     */
    const DxvkDeviceInfo& devicePropertiesExt() const {
      return m_deviceInfo;
    }
    
    /**
     * \brief Supportred device features
     * 
     * Queries the supported device features.
     * \returns Device features
     */
    const DxvkDeviceFeatures& features() const {
      return m_deviceFeatures;
    }
    
    /**
     * \brief Memory properties
     * 
     * Queries the memory types and memory heaps of
     * the device. This is useful for memory allocators.
     * \returns Device memory properties
     */
    VkPhysicalDeviceMemoryProperties memoryProperties() const;
    
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
      const DxvkDeviceFeatures& required) const;
    
    /**
     * \brief Creates a DXVK device
     * 
     * Creates a logical device for this adapter.
     * \param [in] enabledFeatures Device features
     * \returns Device handle
     */
    Rc<DxvkDevice> createDevice(
            DxvkDeviceFeatures  enabledFeatures);
    
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

    DxvkNameSet         m_deviceExtensions;
    DxvkDeviceInfo      m_deviceInfo;
    DxvkDeviceFeatures  m_deviceFeatures;
    
    std::vector<VkQueueFamilyProperties> m_queueFamilies;

    void queryExtensions();
    void queryDeviceInfo();
    void queryDeviceFeatures();
    void queryDeviceQueues();
    
    uint32_t getAdapterIndex() const;

    static void logNameList(const DxvkNameList& names);
    
  };
  
}
