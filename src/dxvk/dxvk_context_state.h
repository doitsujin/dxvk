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
   * Stores some information on which state
   * of the graphics and compute pipelines
   * has changed and/or needs to be updated.
   */
  enum class DxvkContextFlag : uint64_t  {
    GpRenderPassBound,          ///< Render pass is currently bound
    GpXfbActive,                ///< Transform feedback is enabled
    GpClearRenderTargets,       ///< Render targets need to be cleared
    GpDirtyFramebuffer,         ///< Framebuffer binding is out of date
    GpDirtyPipeline,            ///< Graphics pipeline binding is out of date
    GpDirtyPipelineState,       ///< Graphics pipeline needs to be recompiled
    GpDirtyResources,           ///< Graphics pipeline resource bindings are out of date
    GpDirtyDescriptorOffsets,   ///< Graphics descriptor set needs to be rebound
    GpDirtyDescriptorSet,       ///< Graphics descriptor set needs to be updated
    GpDirtyVertexBuffers,       ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,         ///< Index buffer binding are out of date
    GpDirtyXfbBuffers,          ///< Transform feedback buffer bindings are out of date
    GpDirtyXfbCounters,         ///< Counter buffer values are dirty
    GpDirtyBlendConstants,      ///< Blend constants have changed
    GpDirtyDepthBias,           ///< Depth bias has changed
    GpDirtyStencilRef,          ///< Stencil reference has changed
    GpDirtyViewport,            ///< Viewport state has changed
    GpDynamicBlendConstants,    ///< Blend constants are dynamic
    GpDynamicDepthBias,         ///< Depth bias is dynamic
    GpDynamicStencilRef,        ///< Stencil reference is dynamic
    
    CpDirtyPipeline,            ///< Compute pipeline binding are out of date
    CpDirtyPipelineState,       ///< Compute pipeline needs to be recompiled
    CpDirtyResources,           ///< Compute pipeline resource bindings are out of date
    CpDirtyDescriptorOffsets,   ///< Compute descriptor set needs to be rebound
    CpDirtyDescriptorSet,       ///< Compute descriptor set needs to be updated
    
    DirtyDrawBuffer,            ///< Indirect argument buffer is dirty
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;


  struct DxvkIndirectDrawState {
    DxvkBufferSlice argBuffer;
  };
  
  
  struct DxvkVertexInputState {
    DxvkBufferSlice indexBuffer;
    VkIndexType     indexType   = VK_INDEX_TYPE_UINT32;
    uint32_t        bindingMask = 0;
    
    std::array<DxvkBufferSlice, DxvkLimits::MaxNumVertexBindings> vertexBuffers = { };
    std::array<uint32_t,        DxvkLimits::MaxNumVertexBindings> vertexStrides = { };
  };
  
  
  struct DxvkViewportState {
    std::array<VkViewport, DxvkLimits::MaxNumViewports> viewports    = { };
    std::array<VkRect2D,   DxvkLimits::MaxNumViewports> scissorRects = { };
  };


  struct DxvkOutputMergerState {
    std::array<VkClearValue, MaxNumRenderTargets + 1> clearValues = { };
    
    DxvkRenderTargets   renderTargets;
    DxvkRenderPassOps   renderPassOps;
    Rc<DxvkFramebuffer> framebuffer       = nullptr;
  };


  struct DxvkXfbState {
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> buffers;
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> counters;
  };
  
  
  struct DxvkShaderStage {
    Rc<DxvkShader> shader;
  };
  
  
  struct DxvkGraphicsPipelineState {
    DxvkShaderStage vs;
    DxvkShaderStage tcs;
    DxvkShaderStage tes;
    DxvkShaderStage gs;
    DxvkShaderStage fs;

    DxvkGraphicsPipelineStateInfo state;
    DxvkGraphicsPipelineFlags     flags;
    Rc<DxvkGraphicsPipeline>      pipeline;
  };
  
  
  struct DxvkComputePipelineState {
    DxvkShaderStage cs;
    
    DxvkComputePipelineStateInfo  state;
    Rc<DxvkComputePipeline>       pipeline;
  };


  struct DxvkDynamicState {
    DxvkBlendConstants  blendConstants    = { 0.0f, 0.0f, 0.0f, 0.0f };
    DxvkDepthBias       depthBias         = { 0.0f, 0.0f, 0.0f };
    uint32_t            stencilReference  = 0;
  };
  
  
  /**
   * \brief Pipeline state
   * 
   * Stores all bound shaders, resources,
   * and constant pipeline state objects.
   */
  struct DxvkContextState {
    DxvkIndirectDrawState     id;
    DxvkVertexInputState      vi;
    DxvkViewportState         vp;
    DxvkOutputMergerState     om;
    DxvkXfbState              xfb;
    DxvkDynamicState          dyn;
    
    DxvkGraphicsPipelineState gp;
    DxvkComputePipelineState  cp;
  };
  
}