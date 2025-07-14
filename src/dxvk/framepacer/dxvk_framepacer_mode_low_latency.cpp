#include "dxvk_framepacer_mode_low_latency.h"
#include <vulkan/vulkan_core.h>

namespace dxvk {

  bool LowLatencyMode::getDesiredPresentMode( uint32_t& presentMode ) const {
    if (m_mode != LOW_LATENCY_VRR)
      return false;

    presentMode = (uint32_t) VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    return true;
  }

  int32_t LowLatencyMode::getLowLatencyOffset( const DxvkOptions& options ) {
    int32_t offset = options.lowLatencyOffset;

    offset = std::max( -10000, offset );
    offset = std::min(  10000, offset );
    return offset;
  }

}
