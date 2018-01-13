#include "dxvk_hud_devinfo.h"

namespace dxvk::hud {
  
  HudDeviceInfo::HudDeviceInfo(const Rc<DxvkDevice>& device) {
    VkPhysicalDeviceProperties props = device->adapter()->deviceProperties();
    m_deviceName = props.deviceName;
    m_driverVer = str::format("Driver: ",
      VK_VERSION_MAJOR(props.driverVersion), ".",
      VK_VERSION_MINOR(props.driverVersion), ".",
      VK_VERSION_PATCH(props.driverVersion));
    m_vulkanVer = str::format("Vulkan: ",
      VK_VERSION_MAJOR(props.apiVersion), ".",
      VK_VERSION_MINOR(props.apiVersion), ".",
      VK_VERSION_PATCH(props.apiVersion));
  }
  
  
  HudDeviceInfo::~HudDeviceInfo() {
    
  }
  
  
  HudPos HudDeviceInfo::renderText(
    const Rc<DxvkContext>&  context,
          HudTextRenderer&  renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 0xFF, 0xFF, 0xFF, 0xFF },
      m_deviceName);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 24 },
      { 0xFF, 0xFF, 0xFF, 0xFF },
      m_driverVer);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 44 },
      { 0xFF, 0xFF, 0xFF, 0xFF },
      m_vulkanVer);
    
    return HudPos { position.x, position.y + 68 };
  }
  
}