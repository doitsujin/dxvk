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
    GpClearRenderTargets,       ///< Render targets need to be cleared
    GpDirtyFramebuffer,         ///< Framebuffer binding is out of date
    GpDirtyPipeline,            ///< Graphics pipeline binding is out of date
    GpDirtyPipelineState,       ///< Graphics pipeline needs to be recompiled
    GpDirtyResources,           ///< Graphics pipeline resource bindings are out of date
    GpDirtyDescriptorOffsets,   ///< Graphics descriptor set needs to be rebound
    GpDirtyDescriptorSet,       ///< Graphics descriptor set needs to be updated
    GpDirtyVertexBuffers,       ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,         ///< Index buffer binding are out of date
    GpDirtyBlendConstants,      ///< Blend constants have changed
    GpDirtyStencilRef,          ///< Stencil reference has changed
    GpDirtyViewport,            ///< Viewport state has changed
    GpDirtyDepthBias,           ///< Depth bias has changed
    
    CpDirtyPipeline,            ///< Compute pipeline binding are out of date
    CpDirtyPipelineState,       ///< Compute pipeline needs to be recompiled
    CpDirtyResources,           ///< Compute pipeline resource bindings are out of date
    CpDirtyDescriptorOffsets,   ///< Compute descriptor set needs to be rebound
    CpDirtyDescriptorSet,       ///< Compute descriptor set needs to be updated
  };
  
  using DxvkContextFlags = Flags<DxvkContextFlag>;
  
  
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


  struct DxvkDynamicDepthState {
    float depthBiasConstant = 0.0f;
    float depthBiasClamp    = 0.0f;
    float depthBiasSlope    = 0.0f;
  };
  
  
  struct DxvkOutputMergerState {
    std::array<VkClearValue, MaxNumRenderTargets + 1> clearValues = { };
    
    DxvkRenderTargets   renderTargets;
    DxvkRenderPassOps   renderPassOps;
    Rc<DxvkFramebuffer> framebuffer       = nullptr;
    
    DxvkBlendConstants  blendConstants    = { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t            stencilReference  = 0;
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
    Rc<DxvkGraphicsPipeline>      pipeline;
  };
  
  
  struct DxvkComputePipelineState {
    DxvkShaderStage cs;
    
    DxvkComputePipelineStateInfo  state;
    Rc<DxvkComputePipeline>       pipeline;
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
    DxvkDynamicDepthState     ds;
    DxvkOutputMergerState     om;
    
    DxvkGraphicsPipelineState gp;
    DxvkComputePipelineState  cp;
  };
  
}