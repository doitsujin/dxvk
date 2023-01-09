#pragma once

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include "dxvk_state_cache_types.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkPipelineManager;
  class DxvkPipelineWorkers;

  /**
   * \brief State cache
   * 
   * The shader state cache stores state vectors and
   * render pass formats of all pipelines used in a
   * game, which allows DXVK to compile them ahead
   * of time instead of compiling them on the first
   * draw.
   */
  class DxvkStateCache {

  public:

    DxvkStateCache(
            DxvkDevice*           device,
            DxvkPipelineManager*  pipeManager,
            DxvkPipelineWorkers*  pipeWorkers);

    ~DxvkStateCache();

    /**
     * \brief Adds pipeline library to the cache
     *
     * If the pipeline is not already cached, this
     * will write a new pipeline to the cache file.
     * \param [in] shaders Shader keys
     */
    void addPipelineLibrary(
      const DxvkStateCacheKey&              shaders);

    /**
     * \brief Adds a graphics pipeline to the cache
     * 
     * If the pipeline is not already cached, this
     * will write a new pipeline to the cache file.
     * \param [in] shaders Shader keys
     * \param [in] state Graphics pipeline state
     */
    void addGraphicsPipeline(
      const DxvkStateCacheKey&              shaders,
      const DxvkGraphicsPipelineStateInfo&  state);

    /**
     * \brief Registers a newly compiled shader
     * 
     * Makes the shader available to the pipeline
     * compiler, and starts compiling all pipelines
     * for which all shaders become available.
     * \param [in] shader The shader to add
     */
    void registerShader(
      const Rc<DxvkShader>&                 shader);

    /**
     * \brief Explicitly stops worker threads
     */
    void stopWorkers();

  private:

    using WriterItem = DxvkStateCacheEntry;

    struct WorkerItem {
      DxvkGraphicsPipelineShaders gp;
    };

    DxvkDevice*                       m_device;
    DxvkPipelineManager*              m_pipeManager;
    DxvkPipelineWorkers*              m_pipeWorkers;
    bool                              m_enable = false;

    std::vector<DxvkStateCacheEntry>  m_entries;
    std::atomic<bool>                 m_stopThreads = { false };

    dxvk::mutex                       m_entryLock;

    std::unordered_multimap<
      DxvkStateCacheKey, size_t,
      DxvkHash, DxvkEq> m_entryMap;

    std::unordered_multimap<
      DxvkShaderKey, DxvkStateCacheKey,
      DxvkHash, DxvkEq> m_pipelineMap;
    
    std::unordered_map<
      DxvkShaderKey, Rc<DxvkShader>,
      DxvkHash, DxvkEq> m_shaderMap;

    dxvk::mutex                       m_workerLock;
    dxvk::condition_variable          m_workerCond;
    std::queue<WorkerItem>            m_workerQueue;
    dxvk::thread                      m_workerThread;

    dxvk::mutex                       m_writerLock;
    dxvk::condition_variable          m_writerCond;
    std::queue<WriterItem>            m_writerQueue;
    dxvk::thread                      m_writerThread;

    DxvkShaderKey getShaderKey(
      const Rc<DxvkShader>&           shader) const;

    bool getShaderByKey(
      const DxvkShaderKey&            key,
            Rc<DxvkShader>&           shader) const;
    
    void mapPipelineToEntry(
      const DxvkStateCacheKey&        key,
            size_t                    entryId);
    
    void mapShaderToPipeline(
      const DxvkShaderKey&            shader,
      const DxvkStateCacheKey&        key);

    void compilePipelines(
      const WorkerItem&               item);

    bool readCacheFile();

    bool readCacheHeader(
            std::istream&             stream,
            DxvkStateCacheHeader&     header) const;

    bool readCacheEntry(
            uint32_t                  version,
            std::istream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    void writeCacheEntry(
            std::ostream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    void workerFunc();

    void writerFunc();

    void createWorker();

    void createWriter();

    str::path_string getCacheFileName() const;

    std::ifstream openCacheFileForRead() const;

    std::ofstream openCacheFileForWrite(
            bool                      recreate) const;

    std::string getCacheDir() const;

  };

}
