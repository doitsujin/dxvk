#pragma once

#include <optional>

#include "dxvk_barrier.h"
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
    GpRenderPassActive,         ///< Render pass is currently bound
    GpRenderPassSuspended,      ///< Render pass is currently suspended
    GpRenderPassSecondaryCmd,   ///< Render pass uses secondary command buffer
    GpRenderPassSideEffects,    ///< Render pass has side effects
    GpRenderPassNeedsFlush,     ///< Render pass has pending resolves or discards
    GpRenderPassUnsynchronized, ///< Render pass is not fully serialized.
    GpXfbActive,                ///< Transform feedback is enabled
    GpDirtyRenderTargets,       ///< Bound render targets are out of date
    GpDirtyPipeline,            ///< Graphics pipeline binding is out of date
    GpDirtyPipelineState,       ///< Graphics pipeline needs to be recompiled
    GpDirtyVertexBuffers,       ///< Vertex buffer bindings are out of date
    GpDirtyIndexBuffer,         ///< Index buffer binding are out of date
    GpDirtyXfbBuffers,          ///< Transform feedback buffer bindings are out of date
    GpDirtyBlendConstants,      ///< Blend constants have changed
    GpDirtyDepthBias,           ///< Depth bias has changed
    GpDirtyDepthBounds,         ///< Depth bounds have changed
    GpDirtyDepthClip,           ///< Depth clip state has changed
    GpDirtyDepthTest,           ///< Depth test state has changed
    GpDirtyStencilTest,         ///< Stencil test state other than reference has changed
    GpDirtyStencilRef,          ///< Stencil reference has changed
    GpDirtyMultisampleState,    ///< Multisample state has changed
    GpDirtyRasterizerState,     ///< Cull mode and front face have changed
    GpDirtySampleLocations,     ///< Sample locations have changed
    GpDirtyViewport,            ///< Viewport state has changed
    GpDirtySpecConstants,       ///< Graphics spec constants are out of date
    GpDynamicBlendConstants,    ///< Blend constants are dynamic
    GpDynamicDepthBias,         ///< Depth bias is dynamic
    GpDynamicDepthBounds,       ///< Depth bounds are dynamic
    GpDynamicDepthClip,         ///< Depth clip state is dynamic
    GpDynamicDepthTest,         ///< Depth test is dynamic
    GpDynamicStencilTest,       ///< Stencil test state is dynamic
    GpDynamicMultisampleState,  ///< Multisample state is dynamic
    GpDynamicRasterizerState,   ///< Cull mode and front face are dynamic
    GpDynamicSampleLocations,   ///< Sample locations are dynamic
    GpDynamicVertexStrides,     ///< Vertex buffer strides are dynamic
    GpHasPushData,              ///< Graphics pipeline uses push data
    GpIndependentSets,          ///< Graphics pipeline layout was created with independent sets

    CpComputePassActive,        ///< Whether we are inside a compute pass
    CpDirtyPipelineState,       ///< Compute pipeline is out of date
    CpDirtySpecConstants,       ///< Compute spec constants are out of date
    CpHasPushData,              ///< Compute pipeline uses push data

    DirtyDrawBuffer,            ///< Indirect argument buffer is dirty
    DirtyPushData,              ///< Push data needs to be updated

    ForceWriteAfterWriteSync,   ///< Ignores barrier control flags for write-after-write hazards

    Count
  };

  static_assert(uint32_t(DxvkContextFlag::Count) <= 64u);

  using DxvkContextFlags = Flags<DxvkContextFlag>;


  /**
   * \brief Binding model implementation
   */
  enum class DxvkBindingModel : uint32_t {
    Legacy,
    DescriptorBuffer,
    DescriptorHeap,
  };


  /**
   * \brief Context feature bits
   */
  enum class DxvkContextFeature : uint32_t {
    TrackGraphicsPipeline,
    VariableMultisampleRate,
    DebugUtils,
    DirectMultiDraw,
    DescriptorBuffer,
    DescriptorHeap,
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
    // Ignores write-after-write hazard
    ComputeAllowWriteOnlyOverlap  = 0,
    ComputeAllowReadWriteOverlap  = 1,

    GraphicsAllowReadWriteOverlap = 2,
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


  struct DxvkViewport {
    VkViewport viewport = { };
    VkRect2D   scissor  = { };
  };


  struct DxvkViewportState {
    uint32_t viewportCount = 0;
    std::array<VkViewport, DxvkLimits::MaxNumViewports> viewports    = { };
    std::array<VkRect2D,   DxvkLimits::MaxNumViewports> scissorRects = { };
  };


  struct DxvkOutputMergerState {
    DxvkRenderingInfo   renderingInfo;
    DxvkRenderTargets   renderTargets;
    DxvkRenderPassOps   renderPassOps;
    DxvkFramebufferInfo framebufferInfo;
    DxvkAttachmentMask  attachmentMask;
    VkOffset2D          renderAreaLo = { };
    VkOffset2D          renderAreaHi = { };
  };


  struct DxvkPushDataState {
    std::array<char, MaxTotalPushDataSize> constantData = { };
    std::array<char, MaxTotalPushDataSize> resourceData = { };
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
    DxvkDepthBounds             depthBounds             = { 0.0f, 1.0f };
    DxvkDepthStencilState       depthStencilState       = { };
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
  
  
  struct DxvkDeferredResolve {
    Rc<DxvkImageView> imageView;
    uint32_t layerMask = 0u;
    VkResolveModeFlagBits depthMode   = { };
    VkResolveModeFlagBits stencilMode = { };
    VkRenderingAttachmentFlagsKHR flags = 0u;
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
    DxvkPushDataState         pc;
    DxvkXfbState              xfb;
    DxvkDynamicState          dyn;
    
    DxvkGraphicsPipelineState gp;
    DxvkComputePipelineState  cp;
  };


  /**
   * \brief View pair
   *
   * Stores a buffer view and an image view.
   */
  struct DxvkViewPair {
    Rc<DxvkBufferView> bufferView;
    Rc<DxvkImageView> imageView;
  };
  

  /**
   * \brief Deferred clear info
   */
  struct DxvkClearInfo {
    Rc<DxvkImageView> view = nullptr;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentLoadOp loadOpS = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkClearValue clearValue = { };
    VkImageAspectFlags clearAspects = 0;
    VkImageAspectFlags discardAspects = 0;
  };


  /**
   * \brief Deferred clear batch
   */
  class DxvkClearBatch {

  public:

    void add(std::optional<DxvkClearInfo>&& info) {
      if (info)
        m_batch.push_back(std::move(*info));
    }

    std::pair<const DxvkClearInfo*, size_t> getRange() const {
      return std::make_pair(m_batch.begin(), m_batch.size());
    }

    bool empty() const {
      return m_batch.empty();
    }

  private:

    small_vector<DxvkClearInfo, 16u> m_batch;

  };

}
