#pragma once

#include "../util/config/config.h"

#include "dxvk_adapter.h"
#include "dxvk_device_filter.h"
#include "dxvk_extension_provider.h"
#include "dxvk_options.h"

namespace dxvk {

  /**
   * \brief Vulkan instance creation parameters
   */
  struct DxvkInstanceImportInfo {
    PFN_vkGetInstanceProcAddr loaderProc;
    VkInstance instance;
    uint32_t extensionCount;
    const char** extensionNames;
  };


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
     */
    DxvkInstance();

    /**
     * \brief Imports existing Vulkan instance
     */
    explicit DxvkInstance(
      const DxvkInstanceImportInfo& args);

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
     * \brief Enabled instance extensions
     * \returns Enabled instance extensions
     */
    const DxvkInstanceExtensions& extensions() const {
      return m_extensions;
    }
    
  private:

    Config                  m_config;
    DxvkOptions             m_options;

    Rc<vk::LibraryFn>       m_vkl;
    Rc<vk::InstanceFn>      m_vki;
    DxvkInstanceExtensions  m_extensions;

    VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;

    std::vector<DxvkExtensionProvider*> m_extProviders;
    std::vector<Rc<DxvkAdapter>> m_adapters;
    
    void createLibraryLoader(
      const DxvkInstanceImportInfo& args);

    void createInstanceLoader(
      const DxvkInstanceImportInfo& args);

    std::vector<DxvkExt*> getExtensionList(
            DxvkInstanceExtensions& ext,
            bool                    withDebug);

    DxvkNameSet getExtensionSet(
      const DxvkNameList& extensions);

    std::vector<Rc<DxvkAdapter>> queryAdapters();
    
    static void logNameList(const DxvkNameList& names);

    static VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT  messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT         messageTypes,
      const VkDebugUtilsMessengerCallbackDataEXT*   pCallbackData,
            void*                                   pUserData);

  };
  
}
