#pragma once

#include <mutex>
#include <unordered_map>

#include "dxvk_compute.h"
#include "dxvk_graphics.h"

namespace dxvk {

  class DxvkStateCache;

  /**
   * \brief Pipeline count
   * 
   * Stores number of graphics and
   * compute pipelines, individually.
   */
  struct DxvkPipelineCount {
    uint32_t numGraphicsPipelines;
    uint32_t numComputePipelines;
  };
  
  /**
   * \brief Compute pipeline key
   * 
   * Identifier for a compute pipeline object.
   * Consists of the compute shader itself.
   */
  struct DxvkComputePipelineKey {
    Rc<DxvkShader> cs;
  };
  
  
  /**
   * \brief Graphics pipeline key
   * 
   * Identifier for a graphics pipeline object.
   * Consists of all graphics pipeline shaders.
   */
  struct DxvkGraphicsPipelineKey {
    Rc<DxvkShader> vs;
    Rc<DxvkShader> tcs;
    Rc<DxvkShader> tes;
    Rc<DxvkShader> gs;
    Rc<DxvkShader> fs;
  };
  
  
  struct DxvkPipelineKeyHash {
    size_t operator () (const DxvkComputePipelineKey& key) const;
    size_t operator () (const DxvkGraphicsPipelineKey& key) const;
  };
  
  
  struct DxvkPipelineKeyEq {
    bool operator () (const DxvkComputePipelineKey& a, const DxvkComputePipelineKey& b) const;
    bool operator () (const DxvkGraphicsPipelineKey& a, const DxvkGraphicsPipelineKey& b) const;
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
  class DxvkPipelineManager : public RcObject {
    friend class DxvkComputePipeline;
    friend class DxvkGraphicsPipeline;
  public:
    
    DxvkPipelineManager(
      const DxvkDevice*         device,
            DxvkRenderPassPool* passManager);
    
    ~DxvkPipelineManager();
    
    /**
     * \brief Retrieves a compute pipeline object
     * 
     * If a pipeline for the given shader stage object
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] cs Compute shader
     * \returns Compute pipeline object
     */
    Rc<DxvkComputePipeline> createComputePipeline(
      const Rc<DxvkShader>&         cs);
    
    /**
     * \brief Retrieves a graphics pipeline object
     * 
     * If a pipeline for the given shader stage objects
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] vs Vertex shader
     * \param [in] tcs Tessellation control shader
     * \param [in] tes Tessellation evaluation shader
     * \param [in] gs Geometry shader
     * \param [in] fs Fragment shader
     * \returns Graphics pipeline object
     */
    Rc<DxvkGraphicsPipeline> createGraphicsPipeline(
      const Rc<DxvkShader>&         vs,
      const Rc<DxvkShader>&         tcs,
      const Rc<DxvkShader>&         tes,
      const Rc<DxvkShader>&         gs,
      const Rc<DxvkShader>&         fs);
    
    /*
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
     * \brief Retrieves total pipeline count
     * \returns Number of compute/graphics pipelines
     */
    DxvkPipelineCount getPipelineCount() const;
  private:
    
    const DxvkDevice*         m_device;
    Rc<DxvkPipelineCache>     m_cache;
    Rc<DxvkStateCache>        m_stateCache;

    std::atomic<uint32_t>     m_numComputePipelines  = { 0 };
    std::atomic<uint32_t>     m_numGraphicsPipelines = { 0 };
    
    std::mutex m_mutex;
    
    std::unordered_map<
      DxvkComputePipelineKey,
      Rc<DxvkComputePipeline>,
      DxvkPipelineKeyHash,
      DxvkPipelineKeyEq> m_computePipelines;
    
    std::unordered_map<
      DxvkGraphicsPipelineKey,
      Rc<DxvkGraphicsPipeline>,
      DxvkPipelineKeyHash,
      DxvkPipelineKeyEq> m_graphicsPipelines;
    
  };
  
}