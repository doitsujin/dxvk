#include "dxvk_device.h"
#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<DxvkDevice>& device)
  : m_device(device) {
    
  }
  
  
  DxvkContext::~DxvkContext() {
    
  }
  
  
  void DxvkContext::beginRecording(const Rc<DxvkCommandList>& cmdList) {
    m_cmd = cmdList;
    m_cmd->beginRecording();
    
    // The current state of the internal command buffer is
    // undefined, so we have to bind and set up everything
    // before any draw or dispatch command is recorded.
    m_flags.clr(
      DxvkContextFlag::GpRenderPassBound);
    
    m_flags.set(
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyDynamicState,
      DxvkContextFlag::GpDirtyResources,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::CpDirtyPipeline,
      DxvkContextFlag::CpDirtyResources);
  }
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording() {
    this->renderPassEnd();
    
    m_cmd->endRecording();
    return std::exchange(m_cmd, nullptr);
  }
  
  
  void DxvkContext::bindFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    if (m_state.om.framebuffer != fb) {
      this->renderPassEnd();
      m_state.om.framebuffer = fb;
    }
  }
  
  
  void DxvkContext::bindIndexBuffer(
    const DxvkBufferBinding&    buffer,
          VkIndexType           indexType) {
    if (m_state.vi.indexBuffer != buffer
     || m_state.vi.indexType   != indexType) {
      m_state.vi.indexBuffer = buffer;
      m_state.vi.indexType   = indexType;
      
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    }
  }
  
  
  void DxvkContext::bindResourceBuffer(
          VkPipelineBindPoint   pipe,
          uint32_t              slot,
    const DxvkBufferBinding&    buffer) {
    auto rc = this->getShaderResourceSlots(pipe);
    
    if (rc->getShaderResource(slot).bufferSlice != buffer) {
      m_flags.set(this->getResourceDirtyFlag(pipe));
      
      DxvkShaderResourceSlot resource;
      resource.bufferSlice = buffer;
      
      DxvkDescriptorInfo descriptor;
      
      if (buffer.bufferHandle() != VK_NULL_HANDLE)
        descriptor.buffer = buffer.descriptorInfo();
      
      rc->bindShaderResource(slot, resource, descriptor);
    }
  }
  
  
  void DxvkContext::bindResourceTexelBuffer(
          VkPipelineBindPoint   pipe,
          uint32_t              slot,
    const Rc<DxvkBufferView>&   bufferView) {
    auto rc = this->getShaderResourceSlots(pipe);
    
    if (rc->getShaderResource(slot).bufferView != bufferView) {
      m_flags.set(this->getResourceDirtyFlag(pipe));
      
      DxvkShaderResourceSlot resource;
      resource.bufferView = bufferView;
      
      DxvkDescriptorInfo descriptor;
      
      if (bufferView != nullptr)
        descriptor.texelBuffer = bufferView->handle();
      
      rc->bindShaderResource(slot, resource, descriptor);
    }
  }
  
  
  void DxvkContext::bindResourceImage(
          VkPipelineBindPoint   pipe,
          uint32_t              slot,
    const Rc<DxvkImageView>&    image) {
    auto rc = this->getShaderResourceSlots(pipe);
    
    if (rc->getShaderResource(slot).imageView != image) {
      m_flags.set(this->getResourceDirtyFlag(pipe));
      
      DxvkShaderResourceSlot resource;
      resource.imageView = image;
      
      DxvkDescriptorInfo descriptor;
      
      if (image != nullptr) {
        descriptor.image.imageView   = image->handle();
        descriptor.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      }
      
      rc->bindShaderResource(slot, resource, descriptor);
    }
  }
  
  
  void DxvkContext::bindResourceSampler(
          VkPipelineBindPoint   pipe,
          uint32_t              slot,
    const Rc<DxvkSampler>&      sampler) {
    auto rc = this->getShaderResourceSlots(pipe);
    
    if (rc->getShaderResource(slot).sampler != sampler) {
      m_flags.set(this->getResourceDirtyFlag(pipe));
      
      DxvkShaderResourceSlot resource;
      resource.sampler = sampler;
      
      DxvkDescriptorInfo descriptor;
      
      if (sampler != nullptr)
        descriptor.image.sampler = sampler->handle();
      
      rc->bindShaderResource(slot, resource, descriptor);
    }
  }
  
  
  void DxvkContext::bindShader(
          VkShaderStageFlagBits stage,
    const Rc<DxvkShader>&       shader) {
    DxvkShaderStage* shaderStage = nullptr;
    
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:                  shaderStage = &m_state.gp.vs;  break;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    shaderStage = &m_state.gp.tcs; break;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: shaderStage = &m_state.gp.tes; break;
      case VK_SHADER_STAGE_GEOMETRY_BIT:                shaderStage = &m_state.gp.gs;  break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:                shaderStage = &m_state.gp.fs;  break;
      case VK_SHADER_STAGE_COMPUTE_BIT:                 shaderStage = &m_state.cp.cs;  break;
      default: return;
    }
    
    if (shaderStage->shader != shader) {
      shaderStage->shader = shader;
      
      m_flags.set(stage == VK_SHADER_STAGE_COMPUTE_BIT
        ? DxvkContextFlag::CpDirtyPipeline
        : DxvkContextFlag::GpDirtyPipeline);
    }
  }
  
  
  void DxvkContext::bindVertexBuffer(
          uint32_t              binding,
    const DxvkBufferBinding&    buffer,
          uint32_t              stride) {
    if (m_state.vi.vertexBuffers.at(binding) != buffer) {
      m_state.vi.vertexBuffers.at(binding) = buffer;
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    }
    
    if (m_state.vi.vertexStrides.at(binding) != stride) {
      m_state.vi.vertexStrides.at(binding) = stride;
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
  }
  
  
  void DxvkContext::clearColorImage(
    const Rc<DxvkImage>&            image,
    const VkClearColorValue&        value,
    const VkImageSubresourceRange&  subresources) {
    this->renderPassEnd();
    
    if (image->info().layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      m_barriers.accessImage(image, subresources,
        VK_IMAGE_LAYOUT_UNDEFINED, 0, 0,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
      m_barriers.recordCommands(m_cmd);
    }
    
    m_cmd->cmdClearColorImage(image->handle(),
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      &value, 1, &subresources);
    
    m_barriers.accessImage(image, subresources,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(image);
  }
  
  
  void DxvkContext::clearRenderTarget(
    const VkClearAttachment&  attachment,
    const VkClearRect&        clearArea) {
    // We only need the framebuffer to be bound. Flushing the
    // entire pipeline state is not required and might actually
    // cause problems if the current pipeline state is invalid.
    this->renderPassBegin();
    
    m_cmd->cmdClearAttachments(
      1, &attachment, 1, &clearArea);
  }
  
  
  void DxvkContext::copyBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkDeviceSize          numBytes) {
    if (numBytes != 0) {
      VkBufferCopy bufferRegion;
      bufferRegion.srcOffset = srcOffset;
      bufferRegion.dstOffset = dstOffset;
      bufferRegion.size      = numBytes;
      
      m_cmd->cmdCopyBuffer(
        srcBuffer->handle(),
        dstBuffer->handle(),
        1, &bufferRegion);
      
      m_barriers.accessBuffer(
        srcBuffer, srcOffset, numBytes,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        srcBuffer->info().stages,
        srcBuffer->info().access);
      
      m_barriers.accessBuffer(
        dstBuffer, dstOffset, numBytes,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dstBuffer->info().stages,
        dstBuffer->info().access);
      
      m_barriers.recordCommands(m_cmd);
      
      m_cmd->trackResource(dstBuffer);
      m_cmd->trackResource(srcBuffer);
    }
  }
  
  
  void DxvkContext::dispatch(
          uint32_t x,
          uint32_t y,
          uint32_t z) {
    this->commitComputeState();
    
    m_cmd->cmdDispatch(x, y, z);
    
    this->commitComputeBarriers();
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    m_cmd->cmdDraw(
      vertexCount, instanceCount,
      firstVertex, firstInstance);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          uint32_t vertexOffset,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    m_cmd->cmdDrawIndexed(
      indexCount, instanceCount,
      firstIndex, vertexOffset,
      firstInstance);
  }
  
  
  void DxvkContext::initImage(
    const Rc<DxvkImage>&           image,
    const VkImageSubresourceRange& subresources) {
    m_barriers.accessImage(image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED, 0, 0,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
  }
  
  
  void DxvkContext::updateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
    const void*                     data) {
    this->renderPassEnd();
    
    if (size == VK_WHOLE_SIZE)
      size = buffer->info().size;
    
    if (size != 0) {
      if (size <= 65536) {
        m_cmd->cmdUpdateBuffer(
          buffer->handle(),
          offset, size, data);
      } else {
        // TODO implement
        Logger::err("DxvkContext::updateBuffer: Large updates not yet supported");
      }
      
      m_barriers.accessBuffer(
        buffer, offset, size,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        buffer->info().stages,
        buffer->info().access);
      m_barriers.recordCommands(m_cmd);
    }
  }
  
  
  void DxvkContext::setViewports(
          uint32_t            viewportCount,
    const VkViewport*         viewports,
    const VkRect2D*           scissorRects) {
    if (m_state.vp.viewportCount != viewportCount) {
      m_state.vp.viewportCount = viewportCount;
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
    
    for (uint32_t i = 0; i < viewportCount; i++) {
      m_state.vp.viewports.at(i) = viewports[i];
      m_state.vp.scissorRects.at(i) = scissorRects[i];
    }
    
    this->updateViewports();
  }
  
  
  void DxvkContext::setInputAssemblyState(
    const DxvkInputAssemblyState& state) {
    m_state.ia = state;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setInputLayout(
          uint32_t             attributeCount,
    const DxvkVertexAttribute* attributes,
          uint32_t             bindingCount,
    const DxvkVertexBinding*   bindings) {
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    
    m_state.il.numAttributes = attributeCount;
    m_state.il.numBindings   = bindingCount;
    
    for (uint32_t i = 0; i < attributeCount; i++)
      m_state.il.attributes.at(i) = attributes[i];
    
    for (uint32_t i = 0; i < bindingCount; i++)
      m_state.il.bindings.at(i) = bindings[i];
  }
  
  
  void DxvkContext::setRasterizerState(
    const DxvkRasterizerState& state) {
    m_state.rs = state;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setMultisampleState(
    const DxvkMultisampleState& state) {
    m_state.ms = state;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setDepthStencilState(
    const DxvkDepthStencilState& state) {
    m_state.ds = state;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setLogicOpState(
    const DxvkLogicOpState& state) {
    m_state.lo = state;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setBlendMode(
          uint32_t            attachment,
    const DxvkBlendMode&      blendMode) {
    m_state.om.blendModes.at(attachment) = blendMode;
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::renderPassBegin() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound)
     && (m_state.om.framebuffer != nullptr)) {
      m_flags.set(DxvkContextFlag::GpRenderPassBound);
      
      this->transformLayoutsRenderPassBegin(
        m_state.om.framebuffer->renderTargets());
      
      const DxvkFramebufferSize fbSize
        = m_state.om.framebuffer->size();
      
      VkRect2D renderArea;
      renderArea.offset = VkOffset2D { 0, 0 };
      renderArea.extent = VkExtent2D { fbSize.width, fbSize.height };
      
      VkRenderPassBeginInfo info;
      info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      info.pNext                = nullptr;
      info.renderPass           = m_state.om.framebuffer->renderPass();
      info.framebuffer          = m_state.om.framebuffer->handle();
      info.renderArea           = renderArea;
      info.clearValueCount      = 0;
      info.pClearValues         = nullptr;
      
      m_cmd->cmdBeginRenderPass(&info,
        VK_SUBPASS_CONTENTS_INLINE);
      m_cmd->trackResource(
        m_state.om.framebuffer);
    }
  }
  
  
  void DxvkContext::renderPassEnd() {
    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      m_flags.clr(DxvkContextFlag::GpRenderPassBound);
      m_cmd->cmdEndRenderPass();
      
      this->transformLayoutsRenderPassEnd(
        m_state.om.framebuffer->renderTargets());
    }
  }
  
  
  void DxvkContext::updateComputePipeline() {
    if (m_flags.test(DxvkContextFlag::CpDirtyPipeline)) {
      m_flags.clr(DxvkContextFlag::CpDirtyPipeline);
      
      m_state.cp.pipeline = m_device->createComputePipeline(
        m_state.cp.cs.shader);
      
      m_cmd->cmdBindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE,
        m_state.cp.pipeline->getPipelineHandle());
      m_cmd->trackResource(m_state.cp.pipeline);
    }
  }
  
  
  void DxvkContext::updateGraphicsPipeline() {
    if (m_flags.any(DxvkContextFlag::GpDirtyPipeline, DxvkContextFlag::GpDirtyPipelineState)) {
      m_flags.clr(DxvkContextFlag::GpDirtyPipelineState);
      
      if (m_flags.test(DxvkContextFlag::GpDirtyPipeline)) {
        m_flags.clr(DxvkContextFlag::GpDirtyPipeline);
        
        m_state.gp.pipeline = m_device->createGraphicsPipeline(
          m_state.gp.vs.shader, m_state.gp.tcs.shader, m_state.gp.tes.shader,
          m_state.gp.gs.shader, m_state.gp.fs.shader);
      }
      
      DxvkGraphicsPipelineStateInfo gpState;
      
      gpState.iaPrimitiveTopology      = m_state.ia.primitiveTopology;
      gpState.iaPrimitiveRestart       = m_state.ia.primitiveRestart;
      
      gpState.ilAttributeCount         = m_state.il.numAttributes;
      gpState.ilBindingCount           = m_state.il.numBindings;
      
      for (uint32_t i = 0; i < m_state.il.numAttributes; i++) {
        gpState.ilAttributes[i].location = m_state.il.attributes[i].location;
        gpState.ilAttributes[i].binding  = m_state.il.attributes[i].binding;
        gpState.ilAttributes[i].format   = m_state.il.attributes[i].format;
        gpState.ilAttributes[i].offset   = m_state.il.attributes[i].offset;
      }
      
      for (uint32_t i = 0; i < m_state.il.numBindings; i++) {
        gpState.ilBindings[i].binding    = m_state.il.bindings[i].binding;
        gpState.ilBindings[i].inputRate  = m_state.il.bindings[i].inputRate;
        gpState.ilBindings[i].stride     = m_state.vi.vertexStrides.at(i);
      }
      
      gpState.rsEnableDepthClamp       = m_state.rs.enableDepthClamp;
      gpState.rsEnableDiscard          = m_state.rs.enableDiscard;
      gpState.rsPolygonMode            = m_state.rs.polygonMode;
      gpState.rsCullMode               = m_state.rs.cullMode;
      gpState.rsFrontFace              = m_state.rs.frontFace;
      gpState.rsDepthBiasEnable        = m_state.rs.depthBiasEnable;
      gpState.rsDepthBiasConstant      = m_state.rs.depthBiasConstant;
      gpState.rsDepthBiasClamp         = m_state.rs.depthBiasClamp;
      gpState.rsDepthBiasSlope         = m_state.rs.depthBiasSlope;
      gpState.rsViewportCount          = m_state.vp.viewportCount;
      
      // TODO implement multisampling support properly
      gpState.msSampleCount            = VK_SAMPLE_COUNT_1_BIT;
      gpState.msSampleMask             = m_state.om.sampleMask;
      gpState.msEnableAlphaToCoverage  = m_state.ms.enableAlphaToCoverage;
      gpState.msEnableAlphaToOne       = m_state.ms.enableAlphaToOne;
      gpState.msEnableSampleShading    = m_state.ms.enableSampleShading;
      gpState.msMinSampleShading       = m_state.ms.minSampleShading;
      
      gpState.dsEnableDepthTest        = m_state.ds.enableDepthTest;
      gpState.dsEnableDepthWrite       = m_state.ds.enableDepthWrite;
      gpState.dsEnableDepthBounds      = m_state.ds.enableDepthBounds;
      gpState.dsEnableStencilTest      = m_state.ds.enableStencilTest;
      gpState.dsDepthCompareOp         = m_state.ds.depthCompareOp;
      gpState.dsStencilOpFront         = m_state.ds.stencilOpFront;
      gpState.dsStencilOpBack          = m_state.ds.stencilOpBack;
      gpState.dsDepthBoundsMin         = m_state.ds.depthBoundsMin;
      gpState.dsDepthBoundsMax         = m_state.ds.depthBoundsMax;
      
      gpState.omEnableLogicOp          = m_state.lo.enableLogicOp;
      gpState.omLogicOp                = m_state.lo.logicOp;
      gpState.omRenderPass             = m_state.om.framebuffer->renderPass();
      
      const auto& rt = m_state.om.framebuffer->renderTargets();
      
      for (uint32_t i = 0; i < DxvkLimits::MaxNumRenderTargets; i++) {
        if (rt.getColorTarget(i) != nullptr) {
          const DxvkBlendMode& mode = m_state.om.blendModes.at(i);
          
          gpState.omBlendAttachments[i].blendEnable         = mode.enableBlending;
          gpState.omBlendAttachments[i].srcColorBlendFactor = mode.colorSrcFactor;
          gpState.omBlendAttachments[i].dstColorBlendFactor = mode.colorDstFactor;
          gpState.omBlendAttachments[i].colorBlendOp        = mode.colorBlendOp;
          gpState.omBlendAttachments[i].srcAlphaBlendFactor = mode.alphaSrcFactor;
          gpState.omBlendAttachments[i].dstAlphaBlendFactor = mode.alphaDstFactor;
          gpState.omBlendAttachments[i].alphaBlendOp        = mode.alphaBlendOp;
          gpState.omBlendAttachments[i].colorWriteMask      = mode.writeMask;
        }
      }
      
      m_cmd->cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_state.gp.pipeline->getPipelineHandle(gpState));
      m_cmd->trackResource(m_state.gp.pipeline);
    }
  }
  
  
  void DxvkContext::updateComputeShaderResources() {
    if (m_flags.test(DxvkContextFlag::CpDirtyResources)) {
      m_flags.clr(DxvkContextFlag::CpDirtyResources);
      
      auto layout = m_state.cp.pipeline->layout();
      
      m_cmd->bindResourceDescriptors(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        layout->pipelineLayout(),
        layout->descriptorSetLayout(),
        layout->bindingCount(),
        layout->bindings(),
        m_cResources.descriptors());
    }
  }
  
  
  void DxvkContext::updateGraphicsShaderResources() {
    if (m_flags.test(DxvkContextFlag::GpDirtyResources)) {
      m_flags.clr(DxvkContextFlag::GpDirtyResources);
      
      auto layout = m_state.gp.pipeline->layout();
      
      m_cmd->bindResourceDescriptors(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout->pipelineLayout(),
        layout->descriptorSetLayout(),
        layout->bindingCount(),
        layout->bindings(),
        m_gResources.descriptors());
    }
  }
  
  
  void DxvkContext::updateDynamicState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyDynamicState)) {
      m_flags.clr(DxvkContextFlag::GpDirtyDynamicState);
      
      this->updateViewports();
    }
  }
  
  
  void DxvkContext::updateViewports() {
    m_cmd->cmdSetViewport(0, m_state.vp.viewportCount, m_state.vp.viewports.data());
    m_cmd->cmdSetScissor (0, m_state.vp.viewportCount, m_state.vp.scissorRects.data());
  }
  
  
  void DxvkContext::updateIndexBufferBinding() {
    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer)) {
      m_flags.clr(DxvkContextFlag::GpDirtyIndexBuffer);
      
      if (m_state.vi.indexBuffer.bufferHandle() != VK_NULL_HANDLE) {
        m_cmd->cmdBindIndexBuffer(
          m_state.vi.indexBuffer.bufferHandle(),
          m_state.vi.indexBuffer.bufferOffset(),
          m_state.vi.indexType);
        m_cmd->trackResource(
          m_state.vi.indexBuffer.resource());
      }
    }
  }
  
  
  void DxvkContext::updateVertexBufferBindings() {
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers)) {
      m_flags.clr(DxvkContextFlag::GpDirtyVertexBuffers);
      
      for (uint32_t i = 0; i < m_state.vi.vertexBuffers.size(); i++) {
        const DxvkBufferBinding vbo = m_state.vi.vertexBuffers.at(i);
        
        VkBuffer     handle = vbo.bufferHandle();
        VkDeviceSize offset = vbo.bufferOffset();
        
        if (handle != VK_NULL_HANDLE) {
          m_cmd->cmdBindVertexBuffers(i, 1, &handle, &offset);
          m_cmd->trackResource(vbo.resource());
        }
      }
    }
  }
  
  
  void DxvkContext::commitComputeState() {
    this->renderPassEnd();
    this->updateComputePipeline();
    this->updateComputeShaderResources();
  }
  
  
  void DxvkContext::commitGraphicsState() {
    this->renderPassBegin();
    this->updateGraphicsPipeline();
    this->updateDynamicState();
    this->updateIndexBufferBinding();
    this->updateVertexBufferBindings();
    this->updateGraphicsShaderResources();
  }
  
  
  void DxvkContext::commitComputeBarriers() {
    // TODO implement
  }
  
  
  void DxvkContext::transformLayoutsRenderPassBegin(
    const DxvkRenderTargets& renderTargets) {
    // Ensure that all color attachments are in the optimal layout.
    // Any image that is used as a present source requires special
    // care as we cannot use it for reading.
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const Rc<DxvkImageView> target = renderTargets.getColorTarget(i);
      
      if ((target != nullptr)
       && (target->imageInfo().layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
        VkImageLayout srcLayout = target->imageInfo().layout;
        
        if (srcLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
          srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        m_barriers.accessImage(
          target->image(),
          target->subresources(),
          srcLayout,
          target->imageInfo().stages,
          target->imageInfo().access,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
      }
    }
    
    // Transform the depth-stencil view to the optimal layout
    const Rc<DxvkImageView> dsTarget = renderTargets.getDepthTarget();
    
    if ((dsTarget != nullptr)
     && (dsTarget->imageInfo().layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
      m_barriers.accessImage(
        dsTarget->image(),
        dsTarget->subresources(),
        dsTarget->imageInfo().layout,
        dsTarget->imageInfo().stages,
        dsTarget->imageInfo().access,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }
    
    m_barriers.recordCommands(m_cmd);
  }
  
  
  void DxvkContext::transformLayoutsRenderPassEnd(
    const DxvkRenderTargets& renderTargets) {
    // Transform color attachments back to their original layouts and
    // make sure that they can be used for subsequent draw or compute
    // operations. Swap chain images are treated like any other image.
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const Rc<DxvkImageView> target = renderTargets.getColorTarget(i);
      
      if (target != nullptr) {
        m_barriers.accessImage(
          target->image(),
          target->subresources(),
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          target->imageInfo().layout,
          target->imageInfo().stages,
          target->imageInfo().access);
      }
    }
    
    // Transform the depth-stencil attachment back to its original layout.
    const Rc<DxvkImageView> dsTarget = renderTargets.getDepthTarget();
    
    if (dsTarget != nullptr) {
      m_barriers.accessImage(
        dsTarget->image(),
        dsTarget->subresources(),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        dsTarget->imageInfo().layout,
        dsTarget->imageInfo().stages,
        dsTarget->imageInfo().access);
    }
    
    m_barriers.recordCommands(m_cmd);
  }
  
    
  DxvkShaderResourceSlots* DxvkContext::getShaderResourceSlots(VkPipelineBindPoint pipe) {
    switch (pipe) {
      case VK_PIPELINE_BIND_POINT_GRAPHICS: return &m_gResources;
      case VK_PIPELINE_BIND_POINT_COMPUTE : return &m_cResources;
      default: return nullptr;
    }
  }
  
  
  DxvkContextFlag DxvkContext::getResourceDirtyFlag(VkPipelineBindPoint pipe) const {
    switch (pipe) {
      default:
      case VK_PIPELINE_BIND_POINT_GRAPHICS: return DxvkContextFlag::GpDirtyResources;
      case VK_PIPELINE_BIND_POINT_COMPUTE : return DxvkContextFlag::CpDirtyResources;
    }
  }
  
}