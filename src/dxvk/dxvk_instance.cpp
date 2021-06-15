#include <version.h>

#include "dxvk_instance.h"
#include "dxvk_openvr.h"
#include "dxvk_openxr.h"
#include "dxvk_platform_exts.h"

#include <algorithm>

namespace dxvk {
  
  DxvkInstance::DxvkInstance() {
    Logger::info(str::format("Game: ", env::getExeName()));
    Logger::info(str::format("DXVK: ", DXVK_VERSION));

    m_config = Config::getUserConfig();
    m_config.merge(Config::getAppConfig(env::getExePath()));
    m_config.logOptions();

    m_options = DxvkOptions(m_config);

    m_extProviders.push_back(&DxvkPlatformExts::s_instance);

    if (m_options.enableOpenVR)
      m_extProviders.push_back(&VrInstance::s_instance);

    if (m_options.enableOpenXR)
      m_extProviders.push_back(&DxvkXrProvider::s_instance);

    Logger::info("Built-in extension providers:");
    for (const auto& provider : m_extProviders)
      Logger::info(str::format("  ", provider->getName()));

    for (const auto& provider : m_extProviders)
      provider->initInstanceExtensions();

    m_vkl = new vk::LibraryFn();
    m_vki = new vk::InstanceFn(true, this->createInstance());

    m_adapters = this->queryAdapters();

    for (const auto& provider : m_extProviders)
      provider->initDeviceExtensions(this);

    for (uint32_t i = 0; i < m_adapters.size(); i++) {
      for (const auto& provider : m_extProviders) {
        m_adapters[i]->enableExtensions(
          provider->getDeviceExtensions(i));
      }
    }
  }
  
  
  DxvkInstance::~DxvkInstance() {
    
  }
  
  
  Rc<DxvkAdapter> DxvkInstance::enumAdapters(uint32_t index) const {
    return index < m_adapters.size()
      ? m_adapters[index]
      : nullptr;
  }


  Rc<DxvkAdapter> DxvkInstance::findAdapterByLuid(const void* luid) const {
    for (const auto& adapter : m_adapters) {
      const auto& props = adapter->devicePropertiesExt().coreDeviceId;

      if (props.deviceLUIDValid && !std::memcmp(luid, props.deviceLUID, VK_LUID_SIZE))
        return adapter;
    }

    return nullptr;
  }

  
  Rc<DxvkAdapter> DxvkInstance::findAdapterByDeviceId(uint16_t vendorId, uint16_t deviceId) const {
    for (const auto& adapter : m_adapters) {
      const auto& props = adapter->deviceProperties();

      if (props.vendorID == vendorId
       && props.deviceID == deviceId)
        return adapter;
    }

    return nullptr;
  }
  
  
  VkInstance DxvkInstance::createInstance() {
    DxvkInstanceExtensions insExtensions;

    std::vector<DxvkExt*> insExtensionList = {{
      &insExtensions.khrGetSurfaceCapabilities2,
      &insExtensions.khrSurface,
    }};

    // Hide VK_EXT_debug_utils behind an environment variable. This extension
    // adds additional overhead to winevulkan
    if (env::getEnvVar("DXVK_PERF_EVENTS") == "1") {
        insExtensionList.push_back(&insExtensions.extDebugUtils);
    }

    DxvkNameSet extensionsEnabled;
    DxvkNameSet extensionsAvailable = DxvkNameSet::enumInstanceExtensions(m_vkl);
    
    if (!extensionsAvailable.enableExtensions(
          insExtensionList.size(),
          insExtensionList.data(),
          extensionsEnabled))
      throw DxvkError("DxvkInstance: Failed to create instance");

    m_extensions = insExtensions;

    // Enable additional extensions if necessary
    for (const auto& provider : m_extProviders)
      extensionsEnabled.merge(provider->getInstanceExtensions());

    DxvkNameList extensionNameList = extensionsEnabled.toNameList();
    
    Logger::info("Enabled instance extensions:");
    this->logNameList(extensionNameList);

    std::string appName = env::getExeName();
    
    VkApplicationInfo appInfo;
    appInfo.sType                 = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext                 = nullptr;
    appInfo.pApplicationName      = appName.c_str();
    appInfo.applicationVersion    = 0;
    appInfo.pEngineName           = "DXVK";
    appInfo.engineVersion         = VK_MAKE_VERSION(1, 9, 0);
    appInfo.apiVersion            = VK_MAKE_VERSION(1, 1, 0);
    
    VkInstanceCreateInfo info;
    info.sType                    = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext                    = nullptr;
    info.flags                    = 0;
    info.pApplicationInfo         = &appInfo;
    info.enabledLayerCount        = 0;
    info.ppEnabledLayerNames      = nullptr;
    info.enabledExtensionCount    = extensionNameList.count();
    info.ppEnabledExtensionNames  = extensionNameList.names();
    
    VkInstance result = VK_NULL_HANDLE;
    VkResult status = m_vkl->vkCreateInstance(&info, nullptr, &result);

    if (status != VK_SUCCESS)
      throw DxvkError("DxvkInstance::createInstance: Failed to create Vulkan 1.1 instance");
    
    return result;
  }
  
  
  std::vector<Rc<DxvkAdapter>> DxvkInstance::queryAdapters() {
    uint32_t numAdapters = 0;
    if (m_vki->vkEnumeratePhysicalDevices(m_vki->instance(), &numAdapters, nullptr) != VK_SUCCESS)
      throw DxvkError("DxvkInstance::enumAdapters: Failed to enumerate adapters");
    
    std::vector<VkPhysicalDevice> adapters(numAdapters);
    if (m_vki->vkEnumeratePhysicalDevices(m_vki->instance(), &numAdapters, adapters.data()) != VK_SUCCESS)
      throw DxvkError("DxvkInstance::enumAdapters: Failed to enumerate adapters");

    std::vector<VkPhysicalDeviceProperties> deviceProperties(numAdapters);
    DxvkDeviceFilterFlags filterFlags = 0;

    for (uint32_t i = 0; i < numAdapters; i++) {
      m_vki->vkGetPhysicalDeviceProperties(adapters[i], &deviceProperties[i]);

      if (deviceProperties[i].deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU)
        filterFlags.set(DxvkDeviceFilterFlag::SkipCpuDevices);
    }

    DxvkDeviceFilter filter(filterFlags);
    std::vector<Rc<DxvkAdapter>> result;

    for (uint32_t i = 0; i < numAdapters; i++) {
      if (filter.testAdapter(deviceProperties[i]))
        result.push_back(new DxvkAdapter(m_vki, adapters[i]));
    }
    
    std::stable_sort(result.begin(), result.end(),
      [] (const Rc<DxvkAdapter>& a, const Rc<DxvkAdapter>& b) -> bool {
        static const std::array<VkPhysicalDeviceType, 3> deviceTypes = {{
          VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
          VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
          VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
        }};

        uint32_t aRank = deviceTypes.size();
        uint32_t bRank = deviceTypes.size();

        for (uint32_t i = 0; i < std::min(aRank, bRank); i++) {
          if (a->deviceProperties().deviceType == deviceTypes[i]) aRank = i;
          if (b->deviceProperties().deviceType == deviceTypes[i]) bRank = i;
        }

        return aRank < bRank;
      });
    
    if (result.size() == 0) {
      Logger::warn("DXVK: No adapters found. Please check your "
                   "device filter settings and Vulkan setup.");
    }
    
    return result;
  }
  
  
  void DxvkInstance::logNameList(const DxvkNameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }
  
}
