#include "dxvk_device_filter.h"
#include <iomanip>
#include <sstream>

namespace dxvk {
  
  static std::string convertUUID(const uint8_t uuid[VK_UUID_SIZE]) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < VK_UUID_SIZE; ++i)
      stream << std::setw(2) << static_cast<uint32_t>(uuid[i] & 0xff);
    return stream.str();
  }


  DxvkDeviceFilter::DxvkDeviceFilter(
          DxvkDeviceFilterFlags flags,
    const DxvkOptions&          options)
  : m_flags(flags) {
    m_matchDeviceName = env::getEnvVar("DXVK_FILTER_DEVICE_NAME");
    m_matchDeviceUUID = env::getEnvVar("DXVK_FILTER_DEVICE_UUID");

    if (m_matchDeviceName.empty())
      m_matchDeviceName = options.deviceFilter;

    if (!m_matchDeviceName.empty())
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceName);

    if (!m_matchDeviceUUID.empty())
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceUUID);

    if (m_flags.any(DxvkDeviceFilterFlag::MatchDeviceName,
                    DxvkDeviceFilterFlag::MatchDeviceUUID))
      m_flags.clr(DxvkDeviceFilterFlag::SkipCpuDevices);
  }


  DxvkDeviceFilter::~DxvkDeviceFilter() {

  }


  bool DxvkDeviceFilter::testAdapter(DxvkAdapter& adapter) const {
    const auto& properties = adapter.deviceProperties();

    Logger::info(str::format("Found device: ",
      properties.core.properties.deviceName, " (",
      properties.vk12.driverName, " ",
      properties.driverVersion.toString(), ")"));

    std::string compatError;

    if (!adapter.isCompatible(compatError)) {
      Logger::info(str::format("  Skipping: ", compatError));
      return false;
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceName)) {
      if (std::string(properties.core.properties.deviceName).find(m_matchDeviceName) == std::string::npos) {
        Logger::info("  Skipping: Device filter");
        return false;
      }
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceUUID)) {
      std::string uuidStr = convertUUID(properties.vk11.deviceUUID);

      if (uuidStr.find(m_matchDeviceUUID) == std::string::npos) {
        Logger::info("  Skipping: UUID filter");
        return false;
      }
    }

    if (m_flags.test(DxvkDeviceFilterFlag::SkipCpuDevices)) {
      if (properties.core.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        Logger::info("  Skipping: Software driver");
        return false;
      }
    }

    return true;
  }

}
