#include "dxvk_device_filter.h"

namespace dxvk {
  
  DxvkDeviceFilter::DxvkDeviceFilter() {
    m_matchDeviceName = env::getEnvVar("DXVK_FILTER_DEVICE_NAME");
    
    if (m_matchDeviceName.size() != 0)
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceName);
  }
  
  
  DxvkDeviceFilter::~DxvkDeviceFilter() {
    
  }
  
  
  bool DxvkDeviceFilter::testAdapter(
    const Rc<DxvkAdapter>&  adapter) const {
    const auto& deviceProps = adapter->deviceProperties();
    
    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceName)) {
      if (std::string (deviceProps.deviceName).find(m_matchDeviceName) == std::string::npos)
        return false;
    }
      
    return true;
  }
  
}
