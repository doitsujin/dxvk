#pragma once

#include <mutex>
#include <unordered_map>

#include "dxvk_compute.h"
#include "dxvk_hash.h"
#include "dxvk_graphics.h"

namespace dxvk {
  
  /**
   * \brief Pipeline key
   * 
   * Stores a fixed-size set of shaders in order
   * to identify a shader pipeline object.
   */
  template<size_t N>
  class DxvkPipelineKey {
    
  public:
    
    void setShader(
            size_t          id,
      const Rc<DxvkShader>& shader) {
      m_shaders.at(id) = shader;
    }
    
    size_t hash() const {
      std::hash<DxvkShader*> phash;
      
      DxvkHashState state;
      for (size_t i = 0; i < N; i++)
        state.add(phash(m_shaders[i].ptr()));
      return state;
    }
    
    bool operator == (const DxvkPipelineKey& other) const {
      bool result = true;
      for (size_t i = 0; (i < N) && result; i++)
        result &= m_shaders[i] == other.m_shaders[i];
      return result;
    }
    
    bool operator != (const DxvkPipelineKey& other) const {
      return !this->operator == (other);
    }
    
  private:
    
    std::array<Rc<DxvkShader>, N> m_shaders;
    
  };
  
  /**
   * \brief Pipeline manager
   * 
   * Creates and manages pipeline objects
   * for various combinations of shaders.
   */
  class DxvkPipelineManager : public RcObject {
    
  public:
    
    DxvkPipelineManager(
      const Rc<vk::DeviceFn>& vkd);
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
    
    Rc<vk::DeviceFn> m_vkd;
    
    std::mutex m_mutex;
    
    std::unordered_map<
      DxvkPipelineKey<1>,
      Rc<DxvkComputePipeline>,
      DxvkHash> m_computePipelines;
    
    std::unordered_map<
      DxvkPipelineKey<5>,
      Rc<DxvkGraphicsPipeline>,
      DxvkHash> m_graphicsPipelines;
    
  };
  
}