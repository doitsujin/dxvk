#include "dxvk_device_filter.h"
#include <iomanip>
#include <sstream>

namespace dxvk {

    DxvkDeviceFilter::DxvkDeviceFilter(
      DxvkDeviceFilterFlags flags,
      const DxvkOptions& options)
      : m_flags(flags) {
      m_matchDeviceName = env::getEnvVar("DXVK_FILTER_DEVICE_NAME");
      m_matchDeviceUUID = env::getEnvVar("DXVK_FILTER_DEVICE_UUID");

      if (m_matchDeviceName.empty())
        m_matchDeviceName = options.deviceFilter;

      if (!m_matchDeviceName.empty())
        m_flags.set(DxvkDeviceFilterFlag::MatchDeviceName);

      if (!m_matchDeviceUUID.empty())
        m_flags.set(DxvkDeviceFilterFlag::MatchDeviceUUID);
    }

    DxvkDeviceFilter::~DxvkDeviceFilter() { }

    /// 🔧 Conversor de UUID legível (hexadecimal, sem hífens)
    std::string convertUUID(const uint8_t uuid[VK_UUID_SIZE]) {
      std::ostringstream stream;
      stream << std::hex << std::setfill('0');
      for (size_t i = 0; i < VK_UUID_SIZE; ++i)
        stream << std::setw(2) << static_cast<uint32_t>(uuid[i] & 0xff); // Corrige sinais negativos
      return stream.str();
    }

    bool DxvkDeviceFilter::testAdapter(const VkPhysicalDeviceProperties& properties) const {
      if (properties.apiVersion < VK_MAKE_API_VERSION(0, 1, 3, 0)) {
        Logger::warn(str::format("DXVK: Skipping Vulkan ",
                                 VK_API_VERSION_MAJOR(properties.apiVersion), ".",
                                 VK_API_VERSION_MINOR(properties.apiVersion), " adapter: ",
                                 properties.deviceName));
        return false;
      }

      if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceName)) {
        if (std::string(properties.deviceName).find(m_matchDeviceName) == std::string::npos) {
          Logger::warn(str::format("DXVK: Skipping device not matching name filter: ", properties.deviceName));
          return false;
        }
      } else if (m_flags.test(DxvkDeviceFilterFlag::SkipCpuDevices)) {
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
          Logger::warn(str::format("DXVK: Skipping CPU adapter: ", properties.deviceName));
          return false;
        }
      }

      return true;
    }

    bool DxvkDeviceFilter::testCreatedAdapter(const DxvkDeviceInfo& deviceInfo) const {
      if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceUUID)) {
        std::string uuidStr = convertUUID(deviceInfo.coreDeviceId.deviceUUID);
        if (uuidStr.find(m_matchDeviceUUID) == std::string::npos) {
          Logger::warn(str::format("DXVK: Skipping device not matching UUID filter: ", uuidStr));
          return false;
        }
      }

      return true;
    }

}
