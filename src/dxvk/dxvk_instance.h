#pragma once

#include "../util/config/config.h"

#include "dxvk_adapter.h"
#include "dxvk_device_filter.h"
#include "dxvk_extension_provider.h"
#include "dxvk_options.h"

namespace dxvk {

  constexpr uint32_t DxvkVulkanApiVersion = VK_API_VERSION_1_3;

  /**
   * \brief Vulkan instance creation parameters
   */
  struct DxvkInstanceImportInfo {
    PFN_vkGetInstanceProcAddr loaderProc      = nullptr;
    VkInstance                instance        = VK_NULL_HANDLE;
    uint32_t                  extensionCount  = 0u;
    const char**              extensionNames  = nullptr;
  };


  /**
   * \brief Instance extension properties
   */
  struct DxvkInstanceExtensionInfo {
    VkExtensionProperties extDebugUtils               = vk::makeExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkExtensionProperties extSurfaceMaintenance1      = vk::makeExtension(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
    VkExtensionProperties khrGetSurfaceCapabilities2  = vk::makeExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    VkExtensionProperties khrSurface                  = vk::makeExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    VkExtensionProperties khrSurfaceMaintenance1      = vk::makeExtension(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  };


  /**
   * \brief Debug flags
   */
  enum class DxvkDebugFlag : uint32_t {
    Validation        = 0,
    Capture           = 1,
    Markers           = 2,
  };

  using DxvkDebugFlags = Flags<DxvkDebugFlag>;


  /**
   * \brief Instance creation flags
   *
   * These flags will be passed to the app version field of the Vulkan
   * instance, so that drivers can adjust behaviour for some edge cases
   * that are not implementable with Vulkan itself.
   */
  enum class DxvkInstanceFlag : uint32_t {
    /** Enforce D3D9 behaviour for texture coordinate snapping */
    ClientApiIsD3D9,
  };

  using DxvkInstanceFlags = Flags<DxvkInstanceFlag>;


  /**
   * \brief DXVK instance
   * 
   * Manages a Vulkan instance and stores a list
   * of adapters. This also provides methods for
   * device creation.
   */
  class DxvkInstance : public RcObject {
    
  public:

    /**
     * \brief Creates new Vulkan instance
     * \param [in] flags Instance flags
     */
    explicit DxvkInstance(DxvkInstanceFlags flags);

    /**
     * \brief Imports existing Vulkan instance
     * \param [in] flags Instance flags
     */
    explicit DxvkInstance(
      const DxvkInstanceImportInfo& args,
            DxvkInstanceFlags       flags);

    ~DxvkInstance();
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const {
      return m_vki;
    }
    
    /**
     * \brief Vulkan instance handle
     * \returns The instance handle
     */
    VkInstance handle() {
      return m_vki->instance();
    }

     /**
     * \brief Number of adapters
     * 
     * \returns The number of adapters
     */
    size_t adapterCount() {
      return m_adapters.size();
    }
    
    /**
     * \brief Retrieves an adapter
     * 
     * Note that the adapter does not hold
     * a hard reference to the instance.
     * \param [in] index Adapter index
     * \returns The adapter, or \c nullptr.
     */
    Rc<DxvkAdapter> enumAdapters(
            uint32_t      index) const;
    
    /**
     * \brief Finds adapter by LUID
     * 
     * \param [in] luid Pointer to LUID
     * \returns Matching adapter, if any
     */
    Rc<DxvkAdapter> findAdapterByLuid(
      const void*         luid) const;
    
    /**
     * \brief Finds adapter by device IDs
     * 
     * \param [in] vendorId Vendor ID
     * \param [in] deviceId Device ID
     * \returns Matching adapter, if any
     */
    Rc<DxvkAdapter> findAdapterByDeviceId(
            uint16_t      vendorId,
            uint16_t      deviceId) const;
    
    /**
     * \brief Retrieves configuration options
     * 
     * The configuration set contains user-defined
     * options as well as app-specific options.
     * \returns Configuration options
     */
    const Config& config() const {
      return m_config;
    }

    /**
     * \brief DXVK options
     * \returns DXVK options
     */
    const DxvkOptions& options() const {
      return m_options;
    }

    /**
     * \brief Queries extension support
     */
    const DxvkInstanceExtensionInfo& extensions() const {
      return m_extensionInfo;
    }

    /**
     * \brief Instance extension list
     *
     * Returns the list of extensions that the
     * instance was created with, provided by
     * both DXVK and any extension providers.
     * \returns Instance extension name list
     */
    DxvkExtensionList getExtensionList() const {
      return m_extensionList;
    }

    /**
     * \brief Debug flags
     * \returns Debug flags
     */
    DxvkDebugFlags debugFlags() const {
      return m_debugFlags;
    }
    
  private:

    Config                    m_config;
    DxvkOptions               m_options;

    Rc<vk::LibraryFn>         m_vkl = nullptr;
    Rc<vk::InstanceFn>        m_vki = nullptr;

    DxvkInstanceExtensionInfo m_extensionInfo;
    DxvkExtensionList         m_extensionList;

    DxvkDebugFlags            m_debugFlags = 0u;

    VkDebugUtilsMessengerEXT  m_messenger = VK_NULL_HANDLE;

    std::vector<DxvkExtensionProvider*> m_extProviders;
    std::vector<Rc<DxvkAdapter>> m_adapters;

    bool initVulkanLoader(
      const DxvkInstanceImportInfo& args);

    bool initVulkanInstance(
      const DxvkInstanceImportInfo& args,
            DxvkInstanceFlags       flags);

    bool initAdapters();

    static std::vector<VkExtensionProperties*> getExtensionList(
            DxvkInstanceExtensionInfo& extensions);

    static VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT  messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT         messageTypes,
      const VkDebugUtilsMessengerCallbackDataEXT*   pCallbackData,
            void*                                   pUserData);

  };
  
}
