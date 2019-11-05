#include "dxvk_extension_provider.h"

namespace dxvk {

  class DxvkPlatformExts : public DxvkExtensionProvider {

  public:

    /**
     * \brief Extension provider name
     * \returns The extension provider's name
     */
    std::string_view getName();

    /**
     * \brief Query instance extensions
     * \returns Instance extensions
     */
    DxvkNameSet getInstanceExtensions();

    /**
     * \brief Query device extensions
     * 
     * Retrieves the extensions required for a specific
     * physical device. The adapter index should remain
     * the same across multiple Vulkan instances.
     * \param [in] adapterId Adapter index
     */
    DxvkNameSet getDeviceExtensions(
            uint32_t      adapterId);
    
    /**
     * \brief Initializes instance extension set
     * 
     * Should be called before creating
     * the first Vulkan instance.
     */
    void initInstanceExtensions();

    /**
     * \brief Initializes device extension sets
     * 
     * Should be called after setting
     * up the Vulkan physical devices.
     * \param [in] instance DXVK instance
     */
    void initDeviceExtensions(
      const DxvkInstance* instance);
  };

  extern DxvkPlatformExts g_platformInstance;

}