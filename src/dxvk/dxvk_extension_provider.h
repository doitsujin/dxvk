#pragma once

#include "dxvk_include.h"
#include "dxvk_extensions.h"

#include <vector>
#include <string>

namespace dxvk {

  class DxvkInstance;
  class DxvkExtensionProvider;

  /**
   * \brief Extension provider base
   *
   * Abstract interface for extension
   * providers
   */
  class DxvkExtensionProvider {

  public:

    /**
     * \brief Extension provider name
     * \returns The extension provider's name
     */
    virtual std::string_view getName() = 0;

    /**
     * \brief Query instance extensions
     * \returns Instance extensions
     */
    virtual DxvkNameSet getInstanceExtensions() = 0;

    /**
     * \brief Query device extensions
     * 
     * Retrieves the extensions required for a specific
     * physical device. The adapter index should remain
     * the same across multiple Vulkan instances.
     * \param [in] adapterId Adapter index
     */
    virtual DxvkNameSet getDeviceExtensions(
            uint32_t      adapterId) = 0;
    
    /**
     * \brief Initializes instance extension set
     * 
     * Should be called before creating
     * the first Vulkan instance.
     */
    virtual void initInstanceExtensions() = 0;

    /**
     * \brief Initializes device extension sets
     * 
     * Should be called after setting
     * up the Vulkan physical devices.
     * \param [in] instance DXVK instance
     */
    virtual void initDeviceExtensions(
      const DxvkInstance* instance) = 0;

  };

}