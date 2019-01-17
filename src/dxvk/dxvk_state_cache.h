#pragma once

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include "dxvk_pipemanager.h"
#include "dxvk_renderpass.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief State cache entry key
   * 
   * Stores the shader keys for all
   * graphics shader stages. Used to
   * look up cached state entries.
   */
  struct DxvkStateCacheKey {
    DxvkShaderKey vs;
    DxvkShaderKey tcs;
    DxvkShaderKey tes;
    DxvkShaderKey gs;
    DxvkShaderKey fs;
    DxvkShaderKey cs;

    bool eq(const DxvkStateCacheKey& key) const;

    size_t hash() const;
  };

  
  /**
   * \brief State entry
   * 
   * Stores the shaders used in a pipeline, as well
   * as the full state vector, including its render
   * pass format. This also includes a SHA-1 hash
   * that is used as a check sum to verify integrity.
   */
  struct DxvkStateCacheEntry {
    DxvkStateCacheKey             shaders;
    DxvkGraphicsPipelineStateInfo gpState;
    DxvkComputePipelineStateInfo  cpState;
    DxvkRenderPassFormat          format;
    Sha1Hash                      hash;
  };


  /**
   * \brief State cache header
   * 
   * Stores the state cache format version. If an
   * existing cache file is incompatible to the
   * current version, it will be discarded.
   */
  struct DxvkStateCacheHeader {
    char     magic[4]   = { 'D', 'X', 'V', 'K' };
    uint32_t version    = 3;
    uint32_t entrySize  = sizeof(DxvkStateCacheEntry);
  };

  static_assert(sizeof(DxvkStateCacheHeader) == 12);


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

  private:

    using WriterItem = DxvkStateCacheEntry;

    struct WorkerItem {
      Rc<DxvkShader> vs;
      Rc<DxvkShader> tcs;
      Rc<DxvkShader> tes;
      Rc<DxvkShader> gs;
      Rc<DxvkShader> fs;
      Rc<DxvkShader> cs;
    };

    DxvkPipelineManager*              m_pipeManager;
    DxvkRenderPassPool*               m_passManager;

    std::vector<DxvkStateCacheEntry>  m_entries;
    std::atomic<bool>                 m_stopThreads = { false };

    std::mutex                        m_entryLock;

    std::unordered_multimap<
      DxvkStateCacheKey, size_t,
      DxvkHash, DxvkEq> m_entryMap;

    std::unordered_multimap<
      DxvkShaderKey, DxvkStateCacheKey,
      DxvkHash, DxvkEq> m_pipelineMap;
    
    std::unordered_map<
      DxvkShaderKey, Rc<DxvkShader>,
      DxvkHash, DxvkEq> m_shaderMap;

    std::mutex                        m_workerLock;
    std::condition_variable           m_workerCond;
    std::queue<WorkerItem>            m_workerQueue;
    std::vector<dxvk::thread>         m_workerThreads;

    std::mutex                        m_writerLock;
    std::condition_variable           m_writerCond;
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
            std::istream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    void writeCacheEntry(
            std::ostream&             stream, 
            DxvkStateCacheEntry&      entry) const;
    
    bool convertEntryV2(
            DxvkStateCacheEntry&      entry) const;
    
    void workerFunc();

    void writerFunc();

    std::string getCacheFileName() const;

  };

}