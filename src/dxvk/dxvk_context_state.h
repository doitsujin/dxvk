#pragma once

#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_constant_state.h"
#include "dxvk_framebuffer.h"
#include "dxvk_graphics.h"
#include "dxvk_image.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_sampler.h"
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
    GpDirtyPipeline,        ///< Graphics pipeline binding is out of date
    GpDirtyPipelineState,   ///< Graphics pipeline needs to be recompiled
    GpDirtyDynamicState,    ///< Dynamic state needs to be reapplied
    GpDirtyResources,       ///< Graphics pipeline resource bindings are out of date
    GpDirtyVertexBuffers,   ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,     ///< Index buffer binding are out of date
    
    CpDirtyPipeline,        ///< Compute pipeline binding are out of date
    CpDirtyPipelineState,   ///< Compute pipeline needs to be recompiled
    CpDirtyResources,       ///< Compute pipeline resource bindings are out of date
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;
  
  
  struct DxvkVertexInputState {
    DxvkBufferSlice indexBuffer;
    VkIndexType     indexType = VK_INDEX_TYPE_UINT32;
    
    std::array<DxvkBufferSlice,
      DxvkLimits::MaxNumVertexBindings> vertexBuffers;
    std::array<uint32_t,
      DxvkLimits::MaxNumVertexBindings> vertexStrides;
  };
  
  
  struct DxvkViewportState {
    std::array<VkViewport, DxvkLimits::MaxNumViewports> viewports;
    std::array<VkRect2D,   DxvkLimits::MaxNumViewports> scissorRects;
  };
  
  
  struct DxvkOutputMergerState {
    Rc<DxvkFramebuffer> framebuffer;
    
    float     blendConstants[4];
    uint32_t  stencilReference;
  };
  
  
  struct DxvkShaderStage {
    Rc<DxvkShader> shader;
  };
  
  
  struct DxvkGraphicsPipelineState {
    DxvkShaderStage  vs;
    DxvkShaderStage  tcs;
    DxvkShaderStage  tes;
    DxvkShaderStage  gs;
    DxvkShaderStage  fs;
    
    DxvkGraphicsPipelineStateInfo state;
    Rc<DxvkGraphicsPipeline> pipeline;
  };
  
  
  struct DxvkComputePipelineState {
    DxvkBindingState        bs;
    DxvkShaderStage         cs;
    Rc<DxvkComputePipeline> pipeline;
  };
  
  
  /**
   * \brief Pipeline state
   * 
   * Stores all bound shaders, resources,
   * and constant pipeline state objects.
   */
  struct DxvkContextState {
    DxvkVertexInputState      vi;
    DxvkViewportState         vp;
    DxvkOutputMergerState     om;
    
    DxvkGraphicsPipelineState gp;
    DxvkComputePipelineState  cp;
  };
  
}