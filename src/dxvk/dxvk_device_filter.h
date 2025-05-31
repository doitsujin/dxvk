#pragma once

#include "dxvk_adapter.h"
#include "dxvk_options.h"

namespace dxvk {
  
  /**
   * \brief Device filter flags
   * 
   * The device filter flags specify which device
   * properties are considered when testing adapters.
   * If no flags are set, all devices pass the test.
   */
  enum class DxvkDeviceFilterFlag {
    MatchDeviceName   = 0,
    SkipCpuDevices    = 1,
    MatchDeviceUUID   = 3
  };
  
  using DxvkDeviceFilterFlags = Flags<DxvkDeviceFilterFlag>;
  
  
  /**
   * \brief DXVK device filter
   * 
   * Used to select specific Vulkan devices to use
   * with DXVK. This may be useful for games which
   * do not offer an option to select the correct
   * device.
   */
  class DxvkDeviceFilter {
    
  public:
    
    DxvkDeviceFilter(
            DxvkDeviceFilterFlags flags,
      const DxvkOptions&          options);

    ~DxvkDeviceFilter();
    
    /**
     * \brief Tests an adapter
     * 
     * \param [in] properties Adapter properties
     * \returns \c true if the test passes
     */
    bool testAdapter(
      const VkPhysicalDeviceProperties& properties) const;
      /**
     * \brief Tests a created adapter
     *
     * Applies UUID filtering after device is initialized.
     *
     * \param [in] deviceInfo Information about created device
     * \returns \c true if the device passes the UUID filter
     */
    bool testCreatedAdapter(
        const DxvkDeviceInfo& deviceInfo) const;
    
  private:
    
    DxvkDeviceFilterFlags m_flags;
    
    std::string m_matchDeviceName;
    std::string m_matchDeviceUUID;
    
  };
  
}