#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  enum class DxbcOption : uint64_t {
    /// Use the ShaderImageReadWithoutFormat capability.
    /// Enabled by default on GPUs which support this.
    UseStorageImageReadWithoutFormat,
    
    /// Defer kill operation to the end of the shader.
    /// Fixes derivatives that are undefined due to
    /// non-uniform control flow in fragment shaders.
    DeferKill,
  };
  
  using DxbcOptions = Flags<DxbcOption>;
  
  /**
   * \brief Gets app-specific DXBC options
   * 
   * \param [in] appName Application name
   * \returns DXBC options for this application
   */
  DxbcOptions getDxbcAppOptions(const std::string& appName);
  
  /**
   * \brief Gets device-specific options
   * 
   * \param [in] device The device
   * \returns Device options
   */
  DxbcOptions getDxbcDeviceOptions(const Rc<DxvkDevice>& device);
  
}