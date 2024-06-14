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
    GpDirtyVertexBuffers,       ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,         ///< Index buffer binding are out of date
    GpDirtyXfbBuffers,          ///< Transform feedback buffer bindings are out of date
    GpDirtyBlendConstants,      ///< Blend constants have changed
    GpDirtyDepthStencilState,   ///< Depth-stencil state has changed
    GpDirtyDepthBias,           ///< Depth bias has changed
    GpDirtyDepthBounds,         ///< Depth bounds have changed
    GpDirtyStencilRef,          ///< Stencil reference has changed
    GpDirtyMultisampleState,    ///< Multisample state has changed
    GpDirtyRasterizerState,     ///< Cull mode and front face have changed
    GpDirtyViewport,            ///< Viewport state has changed
    GpDirtySpecConstants,       ///< Graphics spec constants are out of date
    GpDynamicBlendConstants,    ///< Blend constants are dynamic
    GpDynamicDepthStencilState, ///< Depth-stencil state is dynamic
    GpDynamicDepthBias,         ///< Depth bias is dynamic
    GpDynamicDepthBounds,       ///< Depth bounds are dynamic
    GpDynamicStencilRef,        ///< Stencil reference is dynamic
    GpDynamicMultisampleState,  ///< Multisample state is dynamic
    GpDynamicRasterizerState,   ///< Cull mode and front face are dynamic
    GpDynamicVertexStrides,     ///< Vertex buffer strides are dynamic
    GpIndependentSets,          ///< Graphics pipeline layout was created with independent sets
    
    CpDirtyPipelineState,       ///< Compute pipeline is out of date
    CpDirtySpecConstants,       ///< Compute spec constants are out of date
    
    DirtyDrawBuffer,            ///< Indirect argument buffer is dirty
    DirtyPushConstants,         ///< Push constant data has changed
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;


  /**
   * \brief Context feature bits
   */
  enum class DxvkContextFeature : uint32_t {
    TrackGraphicsPipeline,
    VariableMultisampleRate,
    IndexBufferRobustness,
    FeatureCount
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
    std::array<uint32_t,        DxvkLimits::MaxNumVertexBindings> vertexExtents = { };
  };
  
  
  struct DxvkViewportState {
    uint32_t viewportCount = 0;
    std::array<VkViewport, DxvkLimits::MaxNumViewports> viewports    = { };
    std::array<VkRect2D,   DxvkLimits::MaxNumViewports> scissorRects = { };
  };


  struct DxvkOutputMergerState {
    DxvkRenderTargets   renderTargets;
    DxvkRenderPassOps   renderPassOps;
    DxvkFramebufferInfo framebufferInfo;
  };


  struct DxvkPushConstantState {
    char data[MaxPushConstantSize];
  };


  struct DxvkXfbState {
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> buffers;
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> counters;
    std::array<DxvkBufferSlice, MaxNumXfbBuffers> activeCounters;
  };
  
  
  struct DxvkSpecConstantState {
    uint32_t                                  mask = 0;
    std::array<uint32_t, MaxNumSpecConstants> data = { };
  };
  
  
  struct DxvkGraphicsPipelineState {
    DxvkGraphicsPipelineShaders   shaders;
    DxvkGraphicsPipelineStateInfo state;
    DxvkGraphicsPipelineFlags     flags;
    DxvkGraphicsPipeline*         pipeline = nullptr;
    DxvkSpecConstantState         constants;
  };
  
  
  struct DxvkComputePipelineState {
    DxvkComputePipelineShaders    shaders;
    DxvkComputePipelineStateInfo  state;
    DxvkComputePipeline*          pipeline = nullptr;
    DxvkSpecConstantState         constants;
  };


  struct DxvkDynamicState {
    DxvkBlendConstants          blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f };
    DxvkDepthBias               depthBias               = { 0.0f, 0.0f, 0.0f };
    DxvkDepthBiasRepresentation depthBiasRepresentation = { VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, false };
    DxvkDepthBounds             depthBounds             = { false, 0.0f, 1.0f };
    uint32_t                    stencilReference        = 0;
    VkCullModeFlags             cullMode                = VK_CULL_MODE_BACK_BIT;
    VkFrontFace                 frontFace               = VK_FRONT_FACE_CLOCKWISE;
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