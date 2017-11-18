#pragma once

#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_framebuffer.h"
#include "dxvk_graphics.h"
#include "dxvk_image.h"
#include "dxvk_limits.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  /**
   * \brief Graphics pipeline state flags
   * 
   * Stores some information on which state of the
   * graphics pipeline has changed and/or needs to
   * be updated.
   */
  enum class DxvkContextFlag : uint64_t  {
    GpRenderPassBound,      ///< Render pass is currently bound
    GpDirtyPipeline,        ///< Graphics pipeline binding are out of date
    GpDirtyPipelineState,   ///< Graphics pipeline state (blending etc.) is dirty
    GpDirtyResources,       ///< Graphics pipeline resource bindings are out of date
    GpDirtyVertexBuffers,   ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,     ///< Index buffer binding are out of date
    
    CpDirtyPipeline,        ///< Compute pipeline binding are out of date
    CpDirtyResources,       ///< Compute pipeline resource bindings are out of date
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;
  
  
  /**
   * \brief Shader state
   * 
   * Stores the active shader and resources
   * for a single shader stage. All stages
   * support the same types of resources.
   */
  struct DxvkShaderStageState {
    Rc<DxvkShader>            shader;
  };
  
  
  /**
   * \brief Input assembly state
   * 
   * Stores the primitive topology
   * and the vertex input layout.
   */
  struct DxvkInputAssemblyState {
    VkPrimitiveTopology       primitiveTopology;
    VkBool32                  primitiveRestart;
    
    uint32_t                  numVertexBindings   = 0;
    uint32_t                  numVertexAttributes = 0;
    
    std::array<VkVertexInputBindingDescription,
      DxvkLimits::MaxNumVertexBuffers> vertexBindings;
    
    std::array<VkVertexInputAttributeDescription,
      DxvkLimits::MaxNumVertexBuffers> vertexAttributes;
  };
  
  
  /**
   * \brief Vertex input state
   * 
   * Stores the currently bound index
   * buffer and vertex buffers.
   */
  struct DxvkVertexInputState {
    Rc<DxvkBuffer>                      indexBuffer;
    std::array<Rc<DxvkBuffer>,
      DxvkLimits::MaxNumVertexBuffers>  vertexBuffers;
  };
  
  
  /**
   * \brief Output merger state
   * 
   * Stores the active framebuffer and the current
   * blend state, as well as the depth stencil state.
   */
  struct DxvkOutputMergerState {
    Rc<DxvkFramebuffer>       framebuffer;
  };
  
  
  /**
   * \brief Pipeline state
   * 
   * Stores all bound shaders, resources,
   * and constant pipeline state objects.
   */
  struct DxvkContextState {
    DxvkShaderStageState      vs;
    DxvkShaderStageState      tcs;
    DxvkShaderStageState      tes;
    DxvkShaderStageState      gs;
    DxvkShaderStageState      fs;
    DxvkShaderStageState      cs;
    
    DxvkInputAssemblyState    ia;
    DxvkVertexInputState      vi;
    DxvkOutputMergerState     om;
    
    Rc<DxvkGraphicsPipeline>  activeGraphicsPipeline;
    Rc<DxvkComputePipeline>   activeComputePipeline;
    
    DxvkContextFlags          flags;
  };
  
}