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

  /**
   * \brief State cache
   * 
   * The shader state cache stores state vectors and
   * render pass formats of all pipelines used in a
   * game, which allows DXVK to compile them ahead
   * of time instead of compiling them on the first
   * draw.
   */
  class DxvkStateCache : public RcObject {

  public:

    DxvkStateCache(
      const DxvkDevice*           device,
            DxvkPipelineManager*  pipeManager,
            DxvkRenderPassPool*   passManager);
    
    ~DxvkStateCache();

    /**
     * Adds a graphics pipeline to the cache
     * 
     * If the pipeline is not already cached, this
     * will write a new pipeline to the cache file.
     * \param [in] shaders Shader keys
     * \param [in] state Graphics pipeline state
     * \param [in] format Render pass format
     */
    void addGraphicsPipeline(
      const DxvkStateCacheKey&              shaders,
      const DxvkGraphicsPipelineStateInfo&  state,
      const DxvkRenderPassFormat&           format);

    /**
     * Adds a compute pipeline to the cache
     * 
     * If the pipeline is not already cached, this
     * will write a new pipeline to the cache file.
     * \param [in] shaders Shader keys
     * \param [in] state Compute pipeline state
     */
    void addComputePipeline(
      const DxvkStateCacheKey&              shaders,
      const DxvkComputePipelineStateInfo&   state);

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
     * \brief Checks whether compiler threads are busy
     * \returns \c true if we're compiling shaders
     */
    bool isCompilingShaders() {
      return m_workerBusy.load() > 0;
    }

  private:

    using WriterItem = DxvkStateCacheEntry;

    struct WorkerItem {
      DxvkGraphicsPipelineShaders gp;
      DxvkComputePipelineShaders  cp;
    };

    DxvkPipelineManager*              m_pipeManager;
    DxvkRenderPassPool*               m_passManager;

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
    std::atomic<uint32_t>             m_workerBusy;
    std::vector<dxvk::thread>         m_workerThreads;

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

    bool readCacheEntryV7(
            uint32_t                  version,
            std::istream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    bool readCacheEntry(
            uint32_t                  version,
            std::istream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    void writeCacheEntry(
            std::ostream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    bool convertEntryV2(
            DxvkStateCacheEntryV4&    entry) const;
    
    bool convertEntryV4(
      const DxvkStateCacheEntryV4&    in,
            DxvkStateCacheEntryV6&    out) const;
    
    bool convertEntryV5(
      const DxvkStateCacheEntryV5&    in,
            DxvkStateCacheEntryV6&    out) const;
    
    bool convertEntryV6(
      const DxvkStateCacheEntryV6&    in,
            DxvkStateCacheEntry&      out) const;
    
    void workerFunc();

    void writerFunc();

    std::wstring getCacheFileName() const;
    
    std::string getCacheDir() const;

    static uint8_t packImageLayout(
            VkImageLayout             layout);

    static VkImageLayout unpackImageLayout(
            uint8_t                   layout);

    static bool validateRenderPassFormat(
      const DxvkRenderPassFormat&     format);

  };

}
