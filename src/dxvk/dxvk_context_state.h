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
  enum class DxvkContextFlag : uint32_t  {
    GpRenderPassBound,          ///< Render pass is currently bound
    GpRenderPassSuspended,      ///< Render pass is currently suspended
    GpXfbActive,                ///< Transform feedback is enabled
    GpDirtyFramebuffer,         ///< Framebuffer binding is out of date
    GpDirtyPipeline,            ///< Graphics pipeline binding is out of date
    GpDirtyPipelineState,       ///< Graphics pipeline needs to be recompiled
    GpDirtyResources,           ///< Graphics pipeline resource bindings are out of date
    GpDirtyDescriptorBinding,   ///< Graphics descriptor set needs to be rebound
    GpDirtyVertexBuffers,       ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,         ///< Index buffer binding are out of date
    GpDirtyXfbBuffers,          ///< Transform feedback buffer bindings are out of date
    GpDirtyBlendConstants,      ///< Blend constants have changed
    GpDirtyDepthBias,           ///< Depth bias has changed
    GpDirtyDepthBounds,         ///< Depth bounds have changed
    GpDirtyStencilRef,          ///< Stencil reference has changed
    GpDirtyViewport,            ///< Viewport state has changed
    GpDynamicBlendConstants,    ///< Blend constants are dynamic
    GpDynamicDepthBias,         ///< Depth bias is dynamic
    GpDynamicDepthBounds,       ///< Depth bounds are dynamic
    GpDynamicStencilRef,        ///< Stencil reference is dynamic
    
    CpDirtyPipeline,            ///< Compute pipeline binding are out of date
    CpDirtyPipelineState,       ///< Compute pipeline needs to be recompiled
    CpDirtyResources,           ///< Compute pipeline resource bindings are out of date
    CpDirtyDescriptorBinding,   ///< Compute descriptor set needs to be rebound
    
    DirtyDrawBuffer,            ///< Indirect argument buffer is dirty
    DirtyPushConstants,         ///< Push constant data has changed
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;


  /**
   * \brief Context feature bits
   */
  enum class DxvkContextFeature {
    NullDescriptors,
    ExtendedDynamicState,
  };

  using DxvkContextFeatures = Flags<DxvkContextFeature>;
  

  /**
   * \brief Barrier control flags
   * 
   * These flags specify what (not) to
   * synchronize implicitly.
   */
  enum class DxvkBarrierControl : uint32_t {
    IgnoreWriteAfterWrite       = 1,
    IgnoreGraphicsBarriers      = 2,
  };

  using DxvkBarrierControlFlags  = Flags<DxvkBarrierControl>;


  struct DxvkIndirectDrawState {
    DxvkBufferSlice argBuffer;
    DxvkBufferSlice cntBuffer;
  };
  
  
  struct DxvkVertexInputState {
    DxvkBufferSlice indexBuffer;
    VkIndexType     indexType   = VK_INDEX_TYPE_UINT32;
    
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


  struct DxvkPushConstantState {
    char data[MaxPushConstantSize];
  };


  struct DxvkXfbState {
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> buffers;
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> counters;
  };
  
  
  struct DxvkGraphicsPipelineState {
    DxvkGraphicsPipelineShaders   shaders;
    DxvkGraphicsPipelineStateInfo state;
    DxvkGraphicsPipelineFlags     flags;
    DxvkGraphicsPipeline*         pipeline = nullptr;
  };
  
  
  struct DxvkComputePipelineState {
    DxvkComputePipelineShaders    shaders;
    DxvkComputePipelineStateInfo  state;
    DxvkComputePipeline*          pipeline = nullptr;
  };


  struct DxvkDynamicState {
    DxvkBlendConstants  blendConstants    = { 0.0f, 0.0f, 0.0f, 0.0f };
    DxvkDepthBias       depthBias         = { 0.0f, 0.0f, 0.0f };
    DxvkDepthBounds     depthBounds       = { false, 0.0f, 1.0f };
    uint32_t            stencilReference  = 0;
  };


  struct DxvkDeferredClear {
    Rc<DxvkImageView> imageView;
    VkImageAspectFlags discardAspects;
    VkImageAspectFlags clearAspects;
    VkClearValue clearValue;
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
    DxvkPushConstantState     pc;
    DxvkXfbState              xfb;
    DxvkDynamicState          dyn;
    
    DxvkGraphicsPipelineState gp;
    DxvkComputePipelineState  cp;
  };
  
}