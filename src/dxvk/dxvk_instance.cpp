#include <version.h>
#include <buildenv.h>

#include "dxvk_instance.h"
#include "dxvk_openvr.h"
#include "dxvk_openxr.h"
#include "dxvk_platform_exts.h"

#include "../wsi/wsi_platform.h"

#include "../vulkan/vulkan_util.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace dxvk {

  DxvkInstance::DxvkInstance(DxvkInstanceFlags flags)
  : DxvkInstance(DxvkInstanceImportInfo(), flags) {

  }


  DxvkInstance::DxvkInstance(const DxvkInstanceImportInfo& args, DxvkInstanceFlags flags) {
    Logger::info(str::format("Game: ", env::getExeName()));
    Logger::info(str::format("DXVK: ", DXVK_VERSION));
    Logger::info(str::format("Build: ", DXVK_TARGET, " ", DXVK_COMPILER, " ", DXVK_COMPILER_VERSION));

    wsi::init();

    m_config = Config::getUserConfig();
    m_config.merge(Config::getAppConfig(env::getExePath()));
    m_config.logOptions();

    m_options = DxvkOptions(m_config);

    // Load Vulkan library
    if (!initVulkanLoader(args))
      throw DxvkError("Failed to load vulkan-1 library.");

    // Initialize extension providers
    m_extProviders.push_back(&DxvkPlatformExts::s_instance);
#ifdef _WIN32
    m_extProviders.push_back(&VrInstance::s_instance);
    m_extProviders.push_back(&DxvkXrProvider::s_instance);
#endif

    Logger::info("Extension providers:");

    for (const auto& provider : m_extProviders) {
      Logger::info(str::format("  ", provider->getName()));
      provider->initInstanceExtensions();
    }

    if (!initVulkanInstance(args, flags))
      throw DxvkError("Failed to initialize DXVK.");

    if (!initAdapters())
      throw DxvkError("Failed to initialize DXVK.");
  }
  
  
  DxvkInstance::~DxvkInstance() {
    if (m_messenger)
      m_vki->vkDestroyDebugUtilsMessengerEXT(m_vki->instance(), m_messenger, nullptr);

    wsi::quit();
  }
  
  
  Rc<DxvkAdapter> DxvkInstance::enumAdapters(uint32_t index) const {
    return index < m_adapters.size()
      ? m_adapters[index]
      : nullptr;
  }


  Rc<DxvkAdapter> DxvkInstance::findAdapterByLuid(const void* luid) const {
    for (const auto& adapter : m_adapters) {
      const auto& vk11 = adapter->deviceProperties().vk11;

      if (vk11.deviceLUIDValid && !std::memcmp(luid, vk11.deviceLUID, VK_LUID_SIZE))
        return adapter;
    }

    return nullptr;
  }

  
  Rc<DxvkAdapter> DxvkInstance::findAdapterByDeviceId(uint16_t vendorId, uint16_t deviceId) const {
    for (const auto& adapter : m_adapters) {
      const auto& props = adapter->deviceProperties();

      if (props.core.properties.vendorID == vendorId
       && props.core.properties.deviceID == deviceId)
        return adapter;
    }

    return nullptr;
  }
  
  
  bool DxvkInstance::initVulkanLoader(const DxvkInstanceImportInfo& args) {
    m_vkl = args.loaderProc
      ? new vk::LibraryFn(args.loaderProc)
      : new vk::LibraryFn();

    return m_vkl->getLoaderProc() != nullptr;
  }


  bool DxvkInstance::initVulkanInstance(const DxvkInstanceImportInfo& args, DxvkInstanceFlags flags) {
    // Query supported instance layers
    std::set<std::string> layersSupported;
    std::set<std::string> layersEnabled;

    uint32_t layerCount = 0u;
    m_vkl->vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    m_vkl->vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (const auto& layer : layers)
      layersSupported.insert(layer.layerName);

    // Query supported instance extensions
    std::set<VkExtensionProperties, vk::SortExtension> extensionsSupported;
    std::set<VkExtensionProperties, vk::SortExtension> extensionsEnabled;

    uint32_t extensionNameCount = 0u;
    m_vkl->vkEnumerateInstanceExtensionProperties(nullptr, &extensionNameCount, nullptr);

    std::vector<VkExtensionProperties> extensionNamesSupported(extensionNameCount);
    m_vkl->vkEnumerateInstanceExtensionProperties(nullptr, &extensionNameCount, extensionNamesSupported.data());

    // When importing an instance, filter by enabled instance extensions
    if (args.instance) {
      for (uint32_t i = 0u; i < args.extensionCount; i++)
        extensionsEnabled.insert(vk::makeExtension(args.extensionNames[i]));
    }

    for (const auto& ext : extensionNamesSupported) {
      bool canEnable = true;

      if (args.instance)
        canEnable = extensionsEnabled.find(ext) != extensionsEnabled.end();

      if (canEnable)
        extensionsSupported.insert(ext);
    }

    // Check which known extensions are supported. We don't have spec
    // version information for imported instances, but that's fine.
    for (const auto& ext : getExtensionList(m_extensionInfo)) {
      auto entry = extensionsSupported.find(*ext);

      if (entry != extensionsSupported.end())
        ext->specVersion = entry->specVersion;
    }

    // Only enable one of the surface maintenance extensions
    if (m_extensionInfo.khrSurfaceMaintenance1.specVersion)
      m_extensionInfo.extSurfaceMaintenance1.specVersion = 0u;

    // Hide debug mode behind an environment variable since it adds
    // significant overhead, and some games will not work with it enabled.
    std::string debugEnv = env::getEnvVar("DXVK_DEBUG");

    bool capture = debugEnv.empty() && (
      env::getEnvVar("ENABLE_VULKAN_RENDERDOC_CAPTURE") == "1" ||
      env::getEnvVar("MESA_VK_TRACE") != "");

    if (debugEnv == "validation")
      m_debugFlags.set(DxvkDebugFlag::Validation);
    else if (debugEnv == "markers")
      m_debugFlags.set(DxvkDebugFlag::Capture, DxvkDebugFlag::Markers);
    else if (debugEnv == "capture" || m_options.enableDebugUtils || capture)
      m_debugFlags.set(DxvkDebugFlag::Capture);

    if (m_debugFlags.isClear()) {
      // Disable any usage of the extension altogether
      m_extensionInfo.extDebugUtils.specVersion = 0u;
    } else {
      Logger::warn("Debug Utils are enabled. May affect performance.");

      if (m_debugFlags.test(DxvkDebugFlag::Validation)) {
        const char* debugLayer = "VK_LAYER_KHRONOS_validation";

        if (layersSupported.find(debugLayer) != layersSupported.end()) {
          layersEnabled.insert(debugLayer);
        } else {
          // This can happen on winevulkan since it does not support layers
          Logger::warn(str::format("Validation layers not found, set VK_INSTANCE_LAYERS=", debugLayer));
        }
      }
    }

    // Log enabled layers, if any
    if (!layersEnabled.empty()) {
      Logger::info("Enabled instance layers:");

      for (const auto& layer : layersEnabled)
        Logger::info(str::format("  ", layer));
    }

    // Generate list of extensions to actually enable
    extensionsEnabled.clear();

    for (const auto& ext : getExtensionList(m_extensionInfo)) {
      if (ext->specVersion)
        extensionsEnabled.insert(*ext);
    }

    for (const auto& provider : m_extProviders) {
      for (const auto& ext : provider->getInstanceExtensions())
        extensionsEnabled.insert(ext);
    }

    Logger::info("Enabled instance extensions:");

    for (const auto& ext : extensionsEnabled) {
      Logger::info(str::format("  ", ext.extensionName));
      m_extensionList.push_back(ext);
    }

    // If necessary, create a new Vulkan instance
    VkInstance instance = args.instance;

    if (!instance) {
      std::string appName = env::getExeName();

      std::vector<const char*> layerNames;
      std::vector<const char*> extensionNames;

      for (const auto& layer : layersEnabled)
        layerNames.push_back(layer.c_str());

      for (const auto& ext : extensionsEnabled)
        extensionNames.push_back(ext.extensionName);

      VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
      appInfo.pApplicationName      = appName.c_str();
      appInfo.applicationVersion    = flags.raw();
      appInfo.pEngineName           = "DXVK";
      appInfo.engineVersion         = VK_MAKE_API_VERSION(0, 2, 7, 1);
      appInfo.apiVersion            = DxvkVulkanApiVersion;

      VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
      info.pApplicationInfo         = &appInfo;
      info.enabledLayerCount        = layerNames.size();
      info.ppEnabledLayerNames      = layerNames.data();
      info.enabledExtensionCount    = extensionNames.size();
      info.ppEnabledExtensionNames  = extensionNames.data();

      VkResult status = m_vkl->vkCreateInstance(&info, nullptr, &instance);

      if (status != VK_SUCCESS) {
        Logger::err("DxvkInstance::createInstance: Failed to create Vulkan instance");
        return false;
      }
    }

    // Create the Vulkan instance loader
    m_vki = new vk::InstanceFn(m_vkl, !args.instance, instance);

    if (m_debugFlags.test(DxvkDebugFlag::Validation)) {
      VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
      messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      messengerInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      messengerInfo.pfnUserCallback = &debugCallback;

      if (m_vki->vkCreateDebugUtilsMessengerEXT(m_vki->instance(), &messengerInfo, nullptr, &m_messenger))
        Logger::err("DxvkInstance::createInstance: Failed to create debug messenger, proceeding without.");
    }

    // Write back debug flags
    return true;
  }


  bool DxvkInstance::initAdapters() {
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

    DxvkDeviceFilter filter(filterFlags, m_options);

    uint32_t numDGPU = 0;
    uint32_t numIGPU = 0;

    for (uint32_t i = 0; i < numAdapters; i++) {
      Rc<DxvkAdapter> adapter = new DxvkAdapter(*this, adapters[i]);

      if (filter.testAdapter(*adapter)) {
        if (deviceProperties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
          numDGPU += 1;
        else if (deviceProperties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
          numIGPU += 1;

        m_adapters.push_back(std::move(adapter));
      }
    }

    std::stable_sort(m_adapters.begin(), m_adapters.end(),
      [] (const Rc<DxvkAdapter>& a, const Rc<DxvkAdapter>& b) -> bool {
        static const std::array<VkPhysicalDeviceType, 3> deviceTypes = {{
          VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
          VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
          VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
        }};

        uint32_t aRank = deviceTypes.size();
        uint32_t bRank = deviceTypes.size();

        for (uint32_t i = 0; i < std::min(aRank, bRank); i++) {
          if (a->deviceProperties().core.properties.deviceType == deviceTypes[i]) aRank = i;
          if (b->deviceProperties().core.properties.deviceType == deviceTypes[i]) bRank = i;
        }

        return aRank < bRank;
      });

    if (m_options.hideIntegratedGraphics && numDGPU > 0 && numIGPU > 0) {
      m_adapters.resize(numDGPU);
      numIGPU = 0;
    }

    if (m_adapters.empty()) {
      Logger::warn(str::format(
        "DXVK: No adapters found. Please check your device filter settings\n"
        "and Vulkan drivers. A Vulkan ",
        VK_API_VERSION_MAJOR(DxvkVulkanApiVersion), ".",
        VK_API_VERSION_MINOR(DxvkVulkanApiVersion), " capable setup is required."));
      return false;
    }

    for (const auto& provider : m_extProviders) {
      provider->initDeviceExtensions(this);
      for (uint32_t i = 0; enumAdapters(i) != nullptr; i++)
        enumAdapters(i)->enableExtensions(provider->getDeviceExtensions(i));
    }

    if (numDGPU == 1u && numIGPU == 1u)
      m_adapters[1u]->linkToDGPU(m_adapters[0u]);

    return true;
  }


  std::vector<VkExtensionProperties*> DxvkInstance::getExtensionList(
          DxvkInstanceExtensionInfo&              extensions) {
    return {{
      &extensions.extDebugUtils,
      &extensions.extSurfaceMaintenance1,
      &extensions.khrGetSurfaceCapabilities2,
      &extensions.khrSurface,
      &extensions.khrSurfaceMaintenance1,
    }};
  }


  VkBool32 VKAPI_CALL DxvkInstance::debugCallback(
          VkDebugUtilsMessageSeverityFlagBitsEXT  messageSeverity,
          VkDebugUtilsMessageTypeFlagsEXT         messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*   pCallbackData,
          void*                                   pUserData) {
    LogLevel logLevel;

    switch (messageSeverity) {
      default:
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    logLevel = LogLevel::Info;  break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: logLevel = LogLevel::Debug; break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: logLevel = LogLevel::Warn;  break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   logLevel = LogLevel::Error; break;
    }

    static const std::array<uint32_t, 9> ignoredIds = {
      // Ignore image format features for depth-compare instructions.
      // These errors are expected in D3D9 and some D3D11 apps.
      0x23259a0d,
      0x4b9d1597,
      0x534c50ad,
      0x9750b479,
      // Ignore vkCmdBindPipeline errors related to dynamic rendering.
      // Validation layers are buggy here and will complain about any
      // command buffer with more than one render pass.
      0x11b37e31,
      0x151f5e5a,
      0x6c16bfb4,
      0xd6d77e1e,
      // Ignore spam about OpSampledImage, validation is wrong here.
      0xa5625282,
    };

    for (auto id : ignoredIds) {
      if (uint32_t(pCallbackData->messageIdNumber) == id)
        return VK_FALSE;
    }

    std::stringstream str;

    if (pCallbackData->pMessageIdName)
      str << pCallbackData->pMessageIdName << ": " << std::endl;

    str << pCallbackData->pMessage;

    Logger::log(logLevel, str.str());
    return VK_FALSE;
  }

}
