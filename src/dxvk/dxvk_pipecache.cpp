#include "dxvk_pipecache.h"

namespace dxvk {
  
  DxvkPipelineCache::DxvkPipelineCache(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd           (vkd),
    m_fileName      (getFileName()),
    m_updateStop    (0),
    m_updateCounter (0),
    m_updateThread  ([this] { this->runThread(); }) {
    auto initialData = this->loadPipelineCache();
    
    VkPipelineCacheCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.pNext            = nullptr;
    info.flags            = 0;
    info.initialDataSize  = initialData.size();
    info.pInitialData     = initialData.data();
    
    if (m_vkd->vkCreatePipelineCache(m_vkd->device(),
        &info, nullptr, &m_handle) != VK_SUCCESS)
      throw DxvkError("DxvkPipelineCache: Failed to create cache");
  }
  
  
  DxvkPipelineCache::~DxvkPipelineCache() {
    // Stop the shader cache update thread
    { std::unique_lock<std::mutex> lock(m_updateMutex);
      m_updateStop = -1;
      m_updateCond.notify_one();
    }
    
    m_updateThread.join();
    
    // Make sure we store the full cache again
    this->storePipelineCache(
      this->getPipelineCache());
    
    m_vkd->vkDestroyPipelineCache(
      m_vkd->device(), m_handle, nullptr);
  }
  
  
  void DxvkPipelineCache::update() {
    m_updateCounter += 1;
  }
  
  
  size_t DxvkPipelineCache::getCacheSize() const {
    size_t cacheSize = 0;
    
    m_vkd->vkGetPipelineCacheData(
      m_vkd->device(), m_handle, &cacheSize, nullptr);
    
    return cacheSize;
  }
  
  
  void DxvkPipelineCache::runThread() {
    uint32_t prevUpdateCounter = 0;
    uint32_t currUpdateCounter = 0;
    
    while (true) {
      // Periodically check whether we need to update once a minute.
      // We don't want to write the full pipeline cache with every
      // single update because that would cause unnecessary load.
      { std::unique_lock<std::mutex> lock(m_updateMutex);
        
        bool exit = m_updateCond.wait_for(lock,
          std::chrono::seconds(60),
          [this] { return m_updateStop != 0; });
        
        if (exit)
          return;
      }
      
      // Only update the cache if new pipelines
      // have been created in the meantime
      currUpdateCounter = m_updateCounter.load();
      
      if (currUpdateCounter != prevUpdateCounter) {
        prevUpdateCounter = currUpdateCounter;
        this->storePipelineCache(this->getPipelineCache());
      }
    }
  }
  
  
  std::vector<char> DxvkPipelineCache::getPipelineCache() const {
    std::vector<char> cacheData;
    
    VkResult status = VK_INCOMPLETE;
    
    while (status == VK_INCOMPLETE) {
      size_t cacheSize = 0;
      
      if (m_vkd->vkGetPipelineCacheData(m_vkd->device(),
            m_handle, &cacheSize, nullptr) != VK_SUCCESS) {
        Logger::warn("DxvkPipelineCache: Failed to retrieve cache size");
        break;
      }
      
      cacheData.resize(cacheSize);
      
      status = m_vkd->vkGetPipelineCacheData(m_vkd->device(),
        m_handle, &cacheSize, cacheData.data());
    }
    
    if (status != VK_SUCCESS) {
      Logger::warn("DxvkPipelineCache: Failed to retrieve cache data");
      cacheData.resize(0);
    }
    
    return cacheData;
  }
  
  
  std::vector<char> DxvkPipelineCache::loadPipelineCache() const {
    std::vector<char> cacheData;
    
    if (m_fileName.size() == 0) {
      Logger::warn("DxvkPipelineCache: Failed to locate cache file");
      return cacheData;
    }
    
    auto cacheFile = std::ifstream(m_fileName,
      std::ios_base::binary | std::ios_base::ate);
    
    if (!cacheFile.good()) {
      Logger::warn("DxvkPipelineCache: Failed to read cache file");
      return cacheData;
    }
    
    std::streamsize cacheSize = cacheFile.tellg();
    cacheFile.seekg(0, std::ios_base::beg);
    cacheData.resize(cacheSize);
    
    if (!cacheFile.read(cacheData.data(), cacheData.size())) {
      Logger::warn("DxvkPipelineCache: Failed to read cache file");
      cacheData.resize(0);
    }
    
    Logger::debug(str::format(
      "DxvkPipelineCache: Read ", cacheData.size(),
      " bytes from ", m_fileName));
    return cacheData;
  }
  
  
  void DxvkPipelineCache::storePipelineCache(const std::vector<char>& cacheData) const {
    if (m_fileName.size() == 0) {
      Logger::warn("DxvkPipelineCache: Failed to locate cache file");
      return;
    }
    
    auto cacheFile = std::ofstream(m_fileName,
      std::ios_base::binary | std::ios_base::trunc);
    
    if (!cacheFile.good()) {
      Logger::warn("DxvkPipelineCache: Failed to open cache file");
      return;
    }
    
    cacheFile.write(cacheData.data(), cacheData.size());
    
    if (!cacheFile.good()) {
      Logger::warn("DxvkPipelineCache: Failed to write shader cache file");
      return;
    }
    
    Logger::debug(str::format(
      "DxvkPipelineCache: Wrote ", cacheData.size(),
      " bytes to ", m_fileName));
  }
  
  
  std::string DxvkPipelineCache::getFileName() {
    const auto exeName = env::getExeName();
    const auto filename = Sha1Hash::compute(
      reinterpret_cast<const uint8_t*>(exeName.c_str()), exeName.size());
    
    const auto temp = env::getTempDirectory();
    
    if (temp.size() != 0)
      return str::format(temp, filename.toString(), ".pipecache");
    
    return std::string();
  }
  
}
