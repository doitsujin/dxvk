
#pragma once

#include <mutex>
#include <queue>
#include <unordered_map>

#include "dxvk_compute.h"
#include "dxvk_graphics.h"
#include "dxvk_state_cache.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Pipeline count
   * 
   * Stores number of graphics and
   * compute pipelines, individually.
   */
  struct DxvkPipelineCount {
    uint32_t numGraphicsPipelines;
    uint32_t numGraphicsLibraries;
    uint32_t numComputePipelines;
  };

  /**
   * \brief Pipeline stats
   */
  struct DxvkPipelineStats {
    std::atomic<uint32_t> numGraphicsPipelines  = { 0u };
    std::atomic<uint32_t> numGraphicsLibraries  = { 0u };
    std::atomic<uint32_t> numComputePipelines   = { 0u };
  };

  /**
   * \brief Pipeline priority
   */
  enum class DxvkPipelinePriority : uint32_t {
    Normal  = 0,
    High    = 1,
  };

  /**
   * \brief Pipeline manager worker threads
   *
   * Spawns worker threads to compile shader pipeline
   * libraries and optimized pipelines asynchronously.
   */
  class DxvkPipelineWorkers {

  public:

    DxvkPipelineWorkers(
            DxvkDevice*                     device);

    ~DxvkPipelineWorkers();

    /**
     * \brief Compiles a pipeline library
     *
     * Asynchronously compiles a basic variant of
     * the pipeline with default compile arguments.
     * Note that pipeline libraries are high priority.
     * \param [in] library The pipeline library
     * \param [in] priority Pipeline priority
     */
    void compilePipelineLibrary(
            DxvkShaderPipelineLibrary*      library,
            DxvkPipelinePriority            priority);

    /**
     * \brief Compiles an optimized compute pipeline
     *
     * \param [in] pipeline Compute pipeline
     * \param [in] state Pipeline state
     */
    void compileComputePipeline(
            DxvkComputePipeline*            pipeline,
      const DxvkComputePipelineStateInfo&   state);

    /**
     * \brief Compiles an optimized graphics pipeline
     *
     * \param [in] pipeline Compute pipeline
     * \param [in] state Pipeline state
     */
    void compileGraphicsPipeline(
            DxvkGraphicsPipeline*           pipeline,
      const DxvkGraphicsPipelineStateInfo&  state);

    /**
     * \brief Checks whether workers are busy
     * \returns \c true if there is unfinished work
     */
    bool isBusy() const;

    /**
     * \brief Stops all worker threads
     *
     * Stops threads and waits for their current work
     * to complete. Queued work will be discarded.
     */
    void stopWorkers();

  private:

    struct PipelineEntry {
      DxvkComputePipeline*          computePipeline;
      DxvkGraphicsPipeline*         graphicsPipeline;
      DxvkComputePipelineStateInfo  computeState;
      DxvkGraphicsPipelineStateInfo graphicsState;
    };

    struct PipelineLibraryEntry {
      DxvkShaderPipelineLibrary*    pipelineLibrary;
    };

    DxvkDevice*                       m_device;

    std::atomic<uint64_t>             m_pendingTasks = { 0ull };

    dxvk::mutex                       m_queueLock;
    dxvk::condition_variable          m_queueCond;
    dxvk::condition_variable          m_queueCondPrioritized;

    std::queue<PipelineLibraryEntry>  m_queuedLibrariesPrioritized;
    std::queue<PipelineLibraryEntry>  m_queuedLibraries;
    std::queue<PipelineEntry>         m_queuedPipelines;

    bool                              m_workersRunning = false;
    std::vector<dxvk::thread>         m_workers;

    void startWorkers();

    void runWorker();

    void runWorkerPrioritized();

  };

  
  /**
   * \brief Pipeline manager
   * 
   * Creates and stores graphics pipelines and compute
   * pipelines for each combination of shaders that is
   * used within the application. This is necessary
   * because DXVK does not expose the concept of shader
   * pipeline objects to the client API.
   */
  class DxvkPipelineManager {
    friend class DxvkComputePipeline;
    friend class DxvkGraphicsPipeline;
    friend class DxvkShaderPipelineLibrary;
  public:
    
    DxvkPipelineManager(
            DxvkDevice*         device);
    
    ~DxvkPipelineManager();
    
    /**
     * \brief Retrieves a compute pipeline object
     * 
     * If a pipeline for the given shader stage object
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] shaders Shaders for the pipeline
     * \returns Compute pipeline object
     */
    DxvkComputePipeline* createComputePipeline(
      const DxvkComputePipelineShaders& shaders);
    
    /**
     * \brief Retrieves a graphics pipeline object
     * 
     * If a pipeline for the given shader stage objects
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] shaders Shaders for the pipeline
     * \returns Graphics pipeline object
     */
    DxvkGraphicsPipeline* createGraphicsPipeline(
      const DxvkGraphicsPipelineShaders& shaders);

    /**
     * \brief Retrieves a vertex input pipeline library
     *
     * \param [in] state Vertex input state
     * \returns Pipeline library object
     */
    DxvkGraphicsPipelineVertexInputLibrary* createVertexInputLibrary(
      const DxvkGraphicsPipelineVertexInputState& state);

    /**
     * \brief Retrieves a fragment output pipeline library
     *
     * \param [in] state Fragment output state
     * \returns Pipeline library object
     */
    DxvkGraphicsPipelineFragmentOutputLibrary* createFragmentOutputLibrary(
      const DxvkGraphicsPipelineFragmentOutputState& state);

    /**
     * \brief Registers a shader
     * 
     * Starts compiling pipelines asynchronously
     * in case the state cache contains state
     * vectors for this shader.
     * \param [in] shader Newly compiled shader
     */
    void registerShader(
      const Rc<DxvkShader>&         shader);

    /**
     * \brief Prioritizes compilation of a given shader
     *
     * Adds the pipeline library for the given shader
     * to the high-priority queue of the background
     * workers to make sure it gets compiled quickly.
     * \param [in] shader Newly compiled shader
     */
    void requestCompileShader(
      const Rc<DxvkShader>&         shader);

    /**
     * \brief Retrieves total pipeline count
     * \returns Number of compute/graphics pipelines
     */
    DxvkPipelineCount getPipelineCount() const;

    /**
     * \brief Checks whether async compiler is busy
     * \returns \c true if shaders are being compiled
     */
    bool isCompilingShaders() const {
      return m_workers.isBusy();
    }

    /**
     * \brief Stops async compiler threads
     */
    void stopWorkerThreads();
    
  private:
    
    DxvkDevice*               m_device;
    DxvkPipelineWorkers       m_workers;
    DxvkStateCache            m_stateCache;
    DxvkPipelineStats         m_stats;
    
    dxvk::mutex m_mutex;
    
    std::unordered_map<
      DxvkBindingSetLayoutKey,
      DxvkBindingSetLayout,
      DxvkHash, DxvkEq> m_descriptorSetLayouts;

    std::unordered_map<
      DxvkBindingLayout,
      DxvkBindingLayoutObjects,
      DxvkHash, DxvkEq> m_pipelineLayouts;

    std::unordered_map<
      DxvkGraphicsPipelineVertexInputState,
      DxvkGraphicsPipelineVertexInputLibrary,
      DxvkHash, DxvkEq> m_vertexInputLibraries;

    std::unordered_map<
      DxvkGraphicsPipelineFragmentOutputState,
      DxvkGraphicsPipelineFragmentOutputLibrary,
      DxvkHash, DxvkEq> m_fragmentOutputLibraries;

    std::unordered_map<
      DxvkShaderPipelineLibraryKey,
      DxvkShaderPipelineLibrary,
      DxvkHash, DxvkEq> m_shaderLibraries;

    std::unordered_map<
      DxvkComputePipelineShaders,
      DxvkComputePipeline,
      DxvkHash, DxvkEq> m_computePipelines;

    std::unordered_map<
      DxvkGraphicsPipelineShaders,
      DxvkGraphicsPipeline,
      DxvkHash, DxvkEq> m_graphicsPipelines;

    DxvkBindingSetLayout* createDescriptorSetLayout(
      const DxvkBindingSetLayoutKey& key);

    DxvkBindingLayoutObjects* createPipelineLayout(
      const DxvkBindingLayout& layout);

    DxvkShaderPipelineLibrary* createPipelineLibrary(
      const Rc<DxvkShader>&     shader);

    DxvkShaderPipelineLibrary* createNullFsPipelineLibrary();

    DxvkShaderPipelineLibrary* findPipelineLibrary(
      const Rc<DxvkShader>&     shader);

    bool canPrecompileShader(
      const Rc<DxvkShader>&     shader) const;

  };
  
}