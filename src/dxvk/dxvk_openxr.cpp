#include "dxvk_instance.h"
#include "dxvk_openxr.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

using PFN___wineopenxr_GetVulkanInstanceExtensions = int (WINAPI *)(uint32_t, uint32_t *, char *);
using PFN___wineopenxr_GetVulkanDeviceExtensions = int (WINAPI *)(uint32_t, uint32_t *, char *);

namespace dxvk {
  
  struct WineXrFunctions {
    PFN___wineopenxr_GetVulkanInstanceExtensions __wineopenxr_GetVulkanInstanceExtensions = nullptr;
    PFN___wineopenxr_GetVulkanDeviceExtensions __wineopenxr_GetVulkanDeviceExtensions = nullptr;
  };
  
  WineXrFunctions g_winexrFunctions;
  DxvkXrProvider DxvkXrProvider::s_instance;

  DxvkXrProvider:: DxvkXrProvider() { }

  DxvkXrProvider::~DxvkXrProvider() { }


  std::string_view DxvkXrProvider::getName() {
    return "OpenXR";
  }
  
  
  DxvkNameSet DxvkXrProvider::getInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return m_insExtensions;
  }


  DxvkNameSet DxvkXrProvider::getDeviceExtensions(uint32_t adapterId) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return m_devExtensions;
  }


  void DxvkXrProvider::initInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_wineOxr)
      m_wineOxr = this->loadLibrary();

    if (!m_wineOxr || m_initializedInsExt)
      return;

    if (!this->loadFunctions()) {
      this->shutdown();
      return;
    }

    m_insExtensions = this->queryInstanceExtensions();
    m_initializedInsExt = true;
  }


  bool DxvkXrProvider::loadFunctions() {
    g_winexrFunctions.__wineopenxr_GetVulkanInstanceExtensions =
        reinterpret_cast<PFN___wineopenxr_GetVulkanInstanceExtensions>(this->getSym("__wineopenxr_GetVulkanInstanceExtensions"));
    g_winexrFunctions.__wineopenxr_GetVulkanDeviceExtensions =
        reinterpret_cast<PFN___wineopenxr_GetVulkanDeviceExtensions>(this->getSym("__wineopenxr_GetVulkanDeviceExtensions"));
    return g_winexrFunctions.__wineopenxr_GetVulkanInstanceExtensions != nullptr
      && g_winexrFunctions.__wineopenxr_GetVulkanDeviceExtensions != nullptr;
  }


  void DxvkXrProvider::initDeviceExtensions(const DxvkInstance* instance) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_wineOxr || m_initializedDevExt)
      return;
    
    m_devExtensions = this->queryDeviceExtensions();
    m_initializedDevExt = true;

    this->shutdown();
  }


  DxvkNameSet DxvkXrProvider::queryInstanceExtensions() const {
    int res;
    uint32_t len;

    res = g_winexrFunctions.__wineopenxr_GetVulkanInstanceExtensions(0, &len, nullptr);
    if (res != 0) {
      Logger::warn("OpenXR: Unable to get required Vulkan instance extensions size");
      return DxvkNameSet();
    }

    std::vector<char> extensionList(len);
    res = g_winexrFunctions.__wineopenxr_GetVulkanInstanceExtensions(len, &len, &extensionList[0]);
    if (res != 0) {
      Logger::warn("OpenXR: Unable to get required Vulkan instance extensions");
      return DxvkNameSet();
    }

    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  DxvkNameSet DxvkXrProvider::queryDeviceExtensions() const {
    int res;

    uint32_t len;
    res = g_winexrFunctions.__wineopenxr_GetVulkanDeviceExtensions(0, &len, nullptr);
    if (res != 0) {
      Logger::warn("OpenXR: Unable to get required Vulkan Device extensions size");
      return DxvkNameSet();
    }

    std::vector<char> extensionList(len);
    res = g_winexrFunctions.__wineopenxr_GetVulkanDeviceExtensions(len, &len, &extensionList[0]);
    if (res != 0) {
      Logger::warn("OpenXR: Unable to get required Vulkan Device extensions");
      return DxvkNameSet();
    }

    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  DxvkNameSet DxvkXrProvider::parseExtensionList(const std::string& str) const {
    DxvkNameSet result;
    
    std::stringstream strstream(str);
    std::string       section;
    
    while (std::getline(strstream, section, ' '))
      result.add(section.c_str());
    
    return result;
  }
  
  
  void DxvkXrProvider::shutdown() {
    if (m_loadedOxrApi)
      this->freeLibrary();
    
    m_loadedOxrApi      = false;
    m_wineOxr = nullptr;
  }


  HMODULE DxvkXrProvider::loadLibrary() {
    HMODULE handle = nullptr;
    if (!(handle = ::GetModuleHandle("wineopenxr.dll"))) {
      handle = ::LoadLibrary("wineopenxr.dll");
      m_loadedOxrApi = handle != nullptr;
    }
    return handle;
  }


  void DxvkXrProvider::freeLibrary() {
    ::FreeLibrary(m_wineOxr);
  }

  
  void* DxvkXrProvider::getSym(const char* sym) {
    return reinterpret_cast<void*>(
      ::GetProcAddress(m_wineOxr, sym));
  }
}
