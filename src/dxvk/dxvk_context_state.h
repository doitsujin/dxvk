#pragma once

#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_framebuffer.h"
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
  enum class DxvkGraphicsPipelineBit : uint64_t  {
    RenderPassBound     =  0, ///< If set, a render pass instance is currently active
    PipelineDirty       =  1, ///< If set, the shader pipeline binding is out of date
    PipelineStateDirty  =  2, ///< If set, another pipeline variant needs to be bound
    DirtyResources      =  3, ///< If set, the descriptor set must be updated
    DirtyVertexBuffers  =  4, ///< If set, the vertex buffer bindings need to be updated
    DirtyIndexBuffer    =  5, ///< If set, the index buffer binding needs to be updated
  };
  
  using DxvkGraphicsPipelineFlags = Flags<DxvkGraphicsPipelineBit>;
  
  
  /**
   * \brief Compute pipeline state flags
   * 
   * Stores information on whether the compute shader
   * or any of its resource bindings have been updated.
   */
  enum class DxvkComputePipelineBit : uint64_t {
    PipelineDirty       =  0, ///< If set, the shader pipeline binding is out of date
    DirtyResources      =  1, ///< If set, the descriptor set must be updated
  };
  
  using DxvkComputePipelineFlags = Flags<DxvkComputePipelineBit>;
  
  
  /**
   * \brief Shader state
   * 
   * Stores the active shader and resources for a single
   * shader stage. This includes sampled textures, uniform
   * buffers, storage buffers and storage images.
   */
  struct DxvkShaderState {
    Rc<DxvkShader> shader;
    
    std::array<DxvkBufferBinding, MaxNumStorageBuffers> boundStorageBuffers;
    std::array<DxvkBufferBinding, MaxNumUniformBuffers> boundUniformBuffers;
  };
  
  
  /**
   * \brief Graphics pipeline state
   * 
   * Stores everything related to graphics
   * operations, including bound resources.
   */
  struct DxvkGraphicsPipelineState {
    DxvkShaderState           vs;
    DxvkShaderState           tcs;
    DxvkShaderState           tes;
    DxvkShaderState           gs;
    DxvkShaderState           fs;
    Rc<DxvkFramebuffer>       fb;
    DxvkGraphicsPipelineFlags flags;
  };
  
  
  /**
   * \brief Compute pipeline state
   * 
   * Stores the active compute pipeline and
   * resources bound to the compute shader.
   */
  struct DxvkComputePipelineState {
    DxvkShaderState           cs;
    Rc<DxvkComputePipeline>   pipeline;
    DxvkComputePipelineFlags  flags;
  };
  
  
  /**
   * \brief DXVK context state
   * 
   * Stores all graphics pipeline state known
   * to DXVK. As in Vulkan, graphics and compute
   * pipeline states are strictly separated.
   */
  struct DxvkContextState {
    DxvkGraphicsPipelineState g;
    DxvkComputePipelineState  c;
  };
  
}