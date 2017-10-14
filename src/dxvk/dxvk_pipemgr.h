#pragma once

#include "dxvk_compute.h"
#include "dxvk_graphics.h"

namespace dxvk {
  
  /**
   * \brief Pipeline manager
   * 
   * Creates and manages pipeline objects
   * for various combinations of shaders.
   */
  class DxvkPipelineManager : public RcObject {
    
  public:
    
    DxvkPipelineManager();
    ~DxvkPipelineManager();
    
    /**
     * \brief Retrieves compute pipeline
     * 
     * Retrieves a compute pipeline object for the given
     * shader. If no such pipeline object exists, a new
     * one will be created.
     * \param [in] cs Compute shader
     */
    Rc<DxvkComputePipeline> getComputePipeline(
      const Rc<DxvkShader>& cs);
    
    /**
     * \brief Retrieves graphics pipeline
     * 
     * Retrieves a graphics pipeline object for the given
     * combination of shaders. If no such pipeline object
     * exists, a new one will be created.
     * \param [in] vs Vertex shader
     * \param [in] tcs Tessellation control shader
     * \param [in] tes Tessellation evaluation shader
     * \param [in] gs Geometry shader
     * \param [in] fs Fragment shader
     * \returns Graphics pipeline
     */
    Rc<DxvkGraphicsPipeline> getGraphicsPipeline(
      const Rc<DxvkShader>& vs,
      const Rc<DxvkShader>& tcs,
      const Rc<DxvkShader>& tes,
      const Rc<DxvkShader>& gs,
      const Rc<DxvkShader>& fs);
    
  private:
    
    std::mutex m_mutex;
    
  };
  
}