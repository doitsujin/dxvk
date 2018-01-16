#pragma once

#include "dxvk_adapter.h"

namespace dxvk {
  
  /**
   * \brief App- and driver-specific options
   */
  enum class DxvkOption : uint64_t {
    /// Assume that the application will not render
    /// multiple polygons with the exact same depth
    /// value. Allows out-of-order rasterization to
    /// be enabled for more rendering modes.
    AssumeNoZfight = 0,
  };
  
  using DxvkOptionSet = Flags<DxvkOption>;
  
  
  /**
   * \brief Option collection
   * 
   * Stores the options enabled for a given
   * device when running a given application.
   */
  class DxvkOptions {
    
  public:
    
    /**
     * \brief Checks whether an option is enabled
     * \returns \c true if the option is enabled
     */
    bool test(DxvkOption opt) const {
      return m_options.test(opt);
    }
    
    /**
     * \brief Sets app-specific options
     * 
     * Application bugs and performance characteristics
     * may require workarounds to be enabled, or allow
     * for certain non-standard optimizations to be used.
     * \param [in] appName Application name
     * \returns Application options
     */
    void adjustAppOptions(
      const std::string&        appName);
    
    /**
     * \brief Adjusts options for a specific driver
     * 
     * Driver bugs and performance characteristics may
     * require some options to be enabled or disabled.
     * \param [in] options Application options
     * \param [in] adapter The adapter
     * \returns Device options
     */
    void adjustDeviceOptions(
      const Rc<DxvkAdapter>&    adapter);
    
    /**
     * \brief Logs enabled options
     * 
     * Informs the user about any options that
     * are enabled. May help with debugging.
     */
    void logOptions() const;
    
  private:
    
    DxvkOptionSet m_options = { 0ull };
    
    void logOption(
            DxvkOption          option,
            const std::string&  name) const;
    
  };
  
}