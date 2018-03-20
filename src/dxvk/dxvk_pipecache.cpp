#include "dxvk_pipecache.h"

namespace dxvk {
  
  DxvkPipelineCache::DxvkPipelineCache(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd),
    m_shaderCacheFile(),
    m_prevUpdate(Clock::now()),
    m_prevSize(0) {
    VkPipelineCacheCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.pNext            = nullptr;
    info.flags            = 0;
    info.initialDataSize  = 0;
    info.pInitialData     = nullptr;
    
    const auto exeName = env::getExeName();
    const auto filename = Sha1Hash::compute(
      reinterpret_cast<const uint8_t*>(exeName.c_str()), exeName.size());
    const auto temp = env::getTempDirectory();
    if (temp.size() != 0)
      m_shaderCacheFile = str::format(temp, filename.toString());
    
    std::vector<char> cache;
    LoadShaderCache(info, cache);
    
    if (m_vkd->vkCreatePipelineCache(m_vkd->device(),
        &info, nullptr, &m_handle) != VK_SUCCESS)
      throw DxvkError("DxvkPipelineCache: Failed to create cache");
    
    size_t cacheSize = 0;
    if (m_vkd->vkGetPipelineCacheData(
        m_vkd->device(), m_handle, &cacheSize, nullptr) != VK_SUCCESS)
      Logger::warn("DxvkPipelineCache: Failed to get pipeline cache size");
    m_prevSize = cacheSize;
  }
  
  
  DxvkPipelineCache::~DxvkPipelineCache() {
    SaveShaderCache();
    
    m_vkd->vkDestroyPipelineCache(
      m_vkd->device(), m_handle, nullptr);
  }
  
  void DxvkPipelineCache::update() {
    const TimePoint now = Clock::now();
    const TimeDiff elapsed = std::chrono::duration_cast<TimeDiff>(now - m_prevUpdate);
    
    size_t cacheSize = 0;
    if (m_vkd->vkGetPipelineCacheData(
        m_vkd->device(), m_handle, &cacheSize, nullptr) != VK_SUCCESS) {
      Logger::warn("DxvkPipelineCache: Failed to get pipeline cache size");
      return;
    }
    
    // update when we have a big difference in size or some time elapsed
    if (cacheSize >= m_prevSize + UpdateSize ||
        (elapsed.count() >= UpdateInterval && cacheSize > m_prevSize)) {
      m_prevSize = cacheSize;
      m_prevUpdate = now;
      SaveShaderCache();
    }
  }
  
  void DxvkPipelineCache::LoadShaderCache(VkPipelineCacheCreateInfo& info, std::vector<char>& cache) const {
    if (m_shaderCacheFile.size() == 0) {
      Logger::warn("DxvkPipelineCache: Caching disabled (temp dir not found)");
      return;
    }
    
    auto cacheFile = std::ifstream(m_shaderCacheFile,
      std::ios_base::binary | std::ios_base::ate);
    
    if (!cacheFile.good())
      return;
    
    std::streamsize cacheSize = cacheFile.tellg();
    cacheFile.seekg(0, std::ios_base::beg);
    cache.resize(cacheSize);
    
    if (!cacheFile.read(cache.data(), cacheSize)) {
      Logger::warn("DxvkPipelineCache: Failed to read shader cache file");
      return;
    }
    
    info.pInitialData = reinterpret_cast<void*>(cache.data());
    info.initialDataSize = cacheSize;
  }
  
  void DxvkPipelineCache::SaveShaderCache() const {
    if (m_shaderCacheFile.size() == 0) {
      Logger::warn("DxvkPipelineCache: Caching disabled (temp dir not found)");
      return;
    }
    
    size_t cacheSize = 0;
    if (m_vkd->vkGetPipelineCacheData(
        m_vkd->device(), m_handle, &cacheSize, nullptr) != VK_SUCCESS) {
      Logger::warn("DxvkPipelineCache: Failed to get pipeline cache size");
      return;
    }
    
    std::vector<char> cache(cacheSize);
    if (m_vkd->vkGetPipelineCacheData(
        m_vkd->device(), m_handle, &cacheSize,
        reinterpret_cast<void*>(cache.data())) != VK_SUCCESS) {
      Logger::warn("DxvkPipelineCache: Failed to get pipeline cache data");
      return;
    }
    
    auto cacheFile = std::ofstream(m_shaderCacheFile,
      std::ios_base::binary | std::ios_base::trunc);
    if (!cacheFile.good()) {
      Logger::warn("DxvkPipelineCache: Failed to open shader cache file");
      return;
    }
    
    cacheFile.write(reinterpret_cast<char*>(cache.data()), cacheSize);
    if (!cacheFile.good()) {
      Logger::warn("DxvkPipelineCache: Failed to write shader cache file");
      return;
    }
    
    Logger::info(str::format("Shader cache written to ", m_shaderCacheFile));
  }
  
}
