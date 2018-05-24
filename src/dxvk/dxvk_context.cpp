#include <cstring>

#include "dxvk_device.h"
#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(
    const Rc<DxvkDevice>&           device,
    const Rc<DxvkPipelineManager>&  pipelineManager,
    const Rc<DxvkMetaClearObjects>& metaClearObjects)
  : m_device    (device),
    m_pipeMgr   (pipelineManager),
    m_metaClear (metaClearObjects) { }
  
  
  DxvkContext::~DxvkContext() {
    
  }
  
  
  void DxvkContext::beginRecording(const Rc<DxvkCommandList>& cmdList) {
    m_cmd = cmdList;
    m_cmd->beginRecording();
    
    // The current state of the internal command buffer is
    // undefined, so we have to bind and set up everything
    // before any draw or dispatch command is recorded.
    m_flags.clr(
      DxvkContextFlag::GpRenderPassBound,
      DxvkContextFlag::GpClearRenderTargets);
    
    m_flags.set(
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyResources,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::CpDirtyPipeline,
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::CpDirtyResources);
  }
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording() {
    this->spillRenderPass();
    
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_OCCLUSION]);
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_PIPELINE_STATISTICS]);
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_TIMESTAMP]);
    
    m_cmd->endRecording();
    return std::exchange(m_cmd, nullptr);
  }
  
  
  void DxvkContext::beginQuery(const DxvkQueryRevision& query) {
    query.query->beginRecording(query.revision);
    
    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      DxvkQueryHandle handle = this->allocQuery(query);
      
      m_cmd->cmdBeginQuery(
        handle.queryPool,
        handle.queryId,
        handle.flags);
    }
    
    this->insertActiveQuery(query);
  }
  
  
  void DxvkContext::endQuery(const DxvkQueryRevision& query) {
    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      DxvkQueryHandle handle = query.query->getHandle();
      
      m_cmd->cmdEndQuery(
        handle.queryPool,
        handle.queryId);
    }
    
    query.query->endRecording(query.revision);
    this->eraseActiveQuery(query);
  }
  
  
  void DxvkContext::bindRenderTargets(const DxvkRenderTargets& targets) {
    m_state.om.renderTargets = targets;
    
    // If necessary, perform clears on the active render targets
    if (m_flags.test(DxvkContextFlag::GpClearRenderTargets))
      this->startRenderPass();
    
    // Set up default render pass ops
    this->resetRenderPassOps(
      m_state.om.renderTargets,
      m_state.om.renderPassOps);
    
    if (m_state.om.framebuffer == nullptr || !m_state.om.framebuffer->hasTargets(targets)) {
      // Create a new framebuffer object next
      // time we start rendering something
      m_flags.set(DxvkContextFlag::GpDirtyFramebuffer);
    } else {
      // Don't redundantly spill the render pass if
      // the same render targets are bound again
      m_flags.clr(DxvkContextFlag::GpDirtyFramebuffer);
    }
  }
  
  
  void DxvkContext::bindIndexBuffer(
    const DxvkBufferSlice&      buffer,
          VkIndexType           indexType) {
    if (!m_state.vi.indexBuffer.matches(buffer)
     || (m_state.vi.indexType != indexType)) {
      m_state.vi.indexBuffer = buffer;
      m_state.vi.indexType   = indexType;
      
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    }
  }
  
  
  void DxvkContext::bindResourceBuffer(
          uint32_t              slot,
    const DxvkBufferSlice&      buffer) {
    if (!m_rc[slot].bufferSlice.matches(buffer)) {
      m_rc[slot].sampler     = nullptr;
      m_rc[slot].imageView   = nullptr;
      m_rc[slot].bufferView  = nullptr;
      m_rc[slot].bufferSlice = buffer;
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
    }
  }
  
  
  void DxvkContext::bindResourceView(
          uint32_t              slot,
    const Rc<DxvkImageView>&    imageView,
    const Rc<DxvkBufferView>&   bufferView) {
    if (m_rc[slot].imageView  != imageView
     || m_rc[slot].bufferView != bufferView) {
      m_rc[slot].sampler     = nullptr;
      m_rc[slot].imageView   = imageView;
      m_rc[slot].bufferView  = bufferView;
      m_rc[slot].bufferSlice = DxvkBufferSlice();
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
    }
  }
  
  
  void DxvkContext::bindResourceSampler(
          uint32_t              slot,
    const Rc<DxvkSampler>&      sampler) {
    if (m_rc[slot].sampler != sampler) {
      m_rc[slot].sampler     = sampler;
      m_rc[slot].imageView   = nullptr;
      m_rc[slot].bufferView  = nullptr;
      m_rc[slot].bufferSlice = DxvkBufferSlice();
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
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
      
      if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        m_flags.set(
          DxvkContextFlag::CpDirtyPipeline,
          DxvkContextFlag::CpDirtyPipelineState,
          DxvkContextFlag::CpDirtyResources);
      } else {
        m_flags.set(
          DxvkContextFlag::GpDirtyPipeline,
          DxvkContextFlag::GpDirtyPipelineState,
          DxvkContextFlag::GpDirtyResources);
      }
    }
  }
  
  
  void DxvkContext::bindVertexBuffer(
          uint32_t              binding,
    const DxvkBufferSlice&      buffer,
          uint32_t              stride) {
    if (!m_state.vi.vertexBuffers[binding].matches(buffer)) {
      m_state.vi.vertexBuffers[binding] = buffer;
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    }
    
    if (m_state.vi.vertexStrides[binding] != stride) {
      m_state.vi.vertexStrides[binding] = stride;
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
  }
  
  
  void DxvkContext::clearBuffer(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          uint32_t              value) {
    this->spillRenderPass();
    
    auto slice = buffer->subSlice(offset, length);
    
    m_cmd->cmdFillBuffer(
      slice.handle(),
      slice.offset(),
      slice.length(),
      value);
    
    m_barriers.accessBuffer(slice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(slice.resource());
  }
  
  
  void DxvkContext::clearBufferView(
    const Rc<DxvkBufferView>&   bufferView,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          VkClearColorValue     value) {
    this->spillRenderPass();
    this->unbindComputePipeline();
    
    // Query pipeline objects to use for this clear operation
    DxvkMetaClearPipeline pipeInfo = m_metaClear->getClearBufferPipeline(
      imageFormatInfo(bufferView->info().format)->flags);
    
    // Create a descriptor set pointing to the view
    VkBufferView viewObject = bufferView->handle();
    
    VkDescriptorSet descriptorSet =
      m_cmd->allocateDescriptorSet(pipeInfo.dsetLayout);
    
    VkWriteDescriptorSet descriptorWrite;
    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.pNext            = nullptr;
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    descriptorWrite.pImageInfo       = nullptr;
    descriptorWrite.pBufferInfo      = nullptr;
    descriptorWrite.pTexelBufferView = &viewObject;
    m_cmd->updateDescriptorSets(1, &descriptorWrite);
    
    // Prepare shader arguments
    DxvkMetaClearArgs pushArgs;
    pushArgs.clearValue = value;
    pushArgs.offset = VkOffset3D {  int32_t(offset), 0, 0 };
    pushArgs.extent = VkExtent3D { uint32_t(length), 1, 1 };
    
    VkExtent3D workgroups = util::computeBlockCount(
      pushArgs.extent, pipeInfo.workgroupSize);
    
    m_cmd->cmdBindPipeline(
      VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(
      VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeInfo.pipeLayout, descriptorSet);
    m_cmd->cmdPushConstants(
      pipeInfo.pipeLayout,
      VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(
      workgroups.width,
      workgroups.height,
      workgroups.depth);
    
    m_barriers.accessBuffer(
      bufferView->physicalSlice(),
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      bufferView->bufferInfo().stages,
      bufferView->bufferInfo().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(bufferView->viewResource());
    m_cmd->trackResource(bufferView->bufferResource());
  }
  
  
  void DxvkContext::clearColorImage(
    const Rc<DxvkImage>&            image,
    const VkClearColorValue&        value,
    const VkImageSubresourceRange&  subresources) {
    this->spillRenderPass();
    
    m_barriers.accessImage(image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED,
      image->info().stages,
      image->info().access,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->cmdClearColorImage(image->handle(),
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      &value, 1, &subresources);
    
    m_barriers.accessImage(image, subresources,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(image);
  }
  
  
  void DxvkContext::clearDepthStencilImage(
    const Rc<DxvkImage>&            image,
    const VkClearDepthStencilValue& value,
    const VkImageSubresourceRange&  subresources) {
    this->spillRenderPass();
    
    m_barriers.accessImage(
      image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED,
      image->info().stages,
      image->info().access,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->cmdClearDepthStencilImage(image->handle(),
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      &value, 1, &subresources);
    
    m_barriers.accessImage(
      image, subresources,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(image);
  }
  
  
  void DxvkContext::clearRenderTarget(
    const Rc<DxvkImageView>&    imageView,
    const VkClearRect&          clearRect,
          VkImageAspectFlags    clearAspects,
    const VkClearValue&         clearValue) {
    this->updateFramebuffer();
    
    // Prepare attachment ops
    DxvkColorAttachmentOps colorOp;
    colorOp.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorOp.loadLayout    = imageView->imageInfo().layout;
    colorOp.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    colorOp.storeLayout   = imageView->imageInfo().layout;
    
    DxvkDepthAttachmentOps depthOp;
    depthOp.loadOpD       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthOp.loadOpS       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthOp.loadLayout    = imageView->imageInfo().layout;
    depthOp.storeOpD      = VK_ATTACHMENT_STORE_OP_STORE;
    depthOp.storeOpS      = VK_ATTACHMENT_STORE_OP_STORE;
    depthOp.storeLayout   = imageView->imageInfo().layout;
    
    if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    
    if (clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_CLEAR;
    
    if (clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_CLEAR;
    
    if (clearAspects == imageView->info().aspect) {
      colorOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      depthOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    // Check whether the render target view is an attachment
    // of the current framebuffer. If not, we need to create
    // a temporary framebuffer.
    int32_t attachmentIndex = -1;
    
    if (m_state.om.framebuffer != nullptr)
      attachmentIndex = m_state.om.framebuffer->findAttachment(imageView);
    
    if (attachmentIndex < 0) {
      this->spillRenderPass();
      
      // Set up and bind a temporary framebuffer
      DxvkRenderTargets attachments;
      DxvkRenderPassOps ops;
      
      if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
        attachments.color[0].view   = imageView;
        attachments.color[0].layout = imageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        ops.colorOps[0] = colorOp;
      } else {
        attachments.depth.view   = imageView;
        attachments.depth.layout = imageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        ops.depthOps = depthOp;
      }
      
      this->renderPassBindFramebuffer(
        m_device->createFramebuffer(attachments),
        ops, 1, &clearValue);
      this->renderPassUnbindFramebuffer();
    } else if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      // Clear the attachment in quesion. For color images,
      // the attachment index for the current subpass is
      // equal to the render pass attachment index.
      VkClearAttachment clearInfo;
      clearInfo.aspectMask      = clearAspects;
      clearInfo.colorAttachment = attachmentIndex;
      clearInfo.clearValue      = clearValue;
      
      m_cmd->cmdClearAttachments(
        1, &clearInfo, 1, &clearRect);
    } else {
      // Perform the clear when starting the render pass
      if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
        m_state.om.renderPassOps.colorOps[attachmentIndex] = colorOp;
      
      if (clearAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
        m_state.om.renderPassOps.depthOps = depthOp;
      
      m_state.om.clearValues[attachmentIndex] = clearValue;
      m_flags.set(DxvkContextFlag::GpClearRenderTargets);
    }
  }
  
  
  void DxvkContext::clearImageView(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkClearColorValue     value) {
    this->spillRenderPass();
    this->unbindComputePipeline();
    
    // Query pipeline objects to use for this clear operation
    DxvkMetaClearPipeline pipeInfo = m_metaClear->getClearImagePipeline(
      imageView->type(), imageFormatInfo(imageView->info().format)->flags);
    
    // Create a descriptor set pointing to the view
    VkDescriptorSet descriptorSet =
      m_cmd->allocateDescriptorSet(pipeInfo.dsetLayout);
    
    VkDescriptorImageInfo viewInfo;
    viewInfo.sampler      = VK_NULL_HANDLE;
    viewInfo.imageView    = imageView->handle();
    viewInfo.imageLayout  = imageView->imageInfo().layout;
    
    VkWriteDescriptorSet descriptorWrite;
    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.pNext            = nullptr;
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.pImageInfo       = &viewInfo;
    descriptorWrite.pBufferInfo      = nullptr;
    descriptorWrite.pTexelBufferView = nullptr;
    m_cmd->updateDescriptorSets(1, &descriptorWrite);
    
    // Prepare shader arguments
    DxvkMetaClearArgs pushArgs;
    pushArgs.clearValue = value;
    pushArgs.offset = offset;
    pushArgs.extent = extent;
    
    VkExtent3D workgroups = util::computeBlockCount(
      pushArgs.extent, pipeInfo.workgroupSize);
    
    if (imageView->type() == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
      workgroups.height = imageView->subresources().layerCount;
    else if (imageView->type() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
      workgroups.depth = imageView->subresources().layerCount;
    
    m_cmd->cmdBindPipeline(
      VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(
      VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeInfo.pipeLayout, descriptorSet);
    m_cmd->cmdPushConstants(
      pipeInfo.pipeLayout,
      VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(
      workgroups.width,
      workgroups.height,
      workgroups.depth);
    
    m_barriers.accessImage(
      imageView->image(),
      imageView->subresources(),
      imageView->imageInfo().layout,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      imageView->imageInfo().layout,
      imageView->imageInfo().stages,
      imageView->imageInfo().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(imageView);
    m_cmd->trackResource(imageView->image());
  }
  
  
  void DxvkContext::copyBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkDeviceSize          numBytes) {
    if (numBytes == 0)
      return;
    
    this->spillRenderPass();
    
    auto dstSlice = dstBuffer->subSlice(dstOffset, numBytes);
    auto srcSlice = srcBuffer->subSlice(srcOffset, numBytes);

    VkBufferCopy bufferRegion;
    bufferRegion.srcOffset = srcSlice.offset();
    bufferRegion.dstOffset = dstSlice.offset();
    bufferRegion.size      = dstSlice.length();

    m_cmd->cmdCopyBuffer(
      srcSlice.handle(),
      dstSlice.handle(),
      1, &bufferRegion);

    m_barriers.accessBuffer(srcSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcBuffer->info().stages,
      srcBuffer->info().access);

    m_barriers.accessBuffer(dstSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstBuffer->info().stages,
      dstBuffer->info().access);

    m_barriers.recordCommands(m_cmd);

    m_cmd->trackResource(dstBuffer->resource());
    m_cmd->trackResource(srcBuffer->resource());
  }
  
  
  void DxvkContext::copyBufferToImage(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
          VkExtent3D            dstExtent,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkExtent2D            srcExtent) {
    this->spillRenderPass();
    
    auto srcSlice = srcBuffer->subSlice(srcOffset, 0);
    
    VkImageSubresourceRange dstSubresourceRange = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel, 1,
      dstSubresource.baseArrayLayer,
      dstSubresource.layerCount };
    
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      dstImage->mipLevelExtent(dstSubresource.mipLevel) == dstExtent
        ? VK_IMAGE_LAYOUT_UNDEFINED
        : dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access,
      dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
    VkBufferImageCopy copyRegion;
    copyRegion.bufferOffset       = srcSlice.offset();
    copyRegion.bufferRowLength    = srcExtent.width;
    copyRegion.bufferImageHeight  = srcExtent.height;
    copyRegion.imageSubresource   = dstSubresource;
    copyRegion.imageOffset        = dstOffset;
    copyRegion.imageExtent        = dstExtent;
    
    m_cmd->cmdCopyBufferToImage(
      srcSlice.handle(),
      dstImage->handle(),
      dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      1, &copyRegion);
    
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);
    m_barriers.accessBuffer(srcSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcBuffer->info().stages,
      srcBuffer->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(dstImage);
    m_cmd->trackResource(srcSlice.resource());
  }
  
  
  void DxvkContext::copyImage(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    this->spillRenderPass();
    
    VkImageSubresourceRange dstSubresourceRange = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel, 1,
      dstSubresource.baseArrayLayer,
      dstSubresource.layerCount };
    
    VkImageSubresourceRange srcSubresourceRange = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel, 1,
      srcSubresource.baseArrayLayer,
      srcSubresource.layerCount };
    
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      dstImage->mipLevelExtent(dstSubresource.mipLevel) == extent
        ? VK_IMAGE_LAYOUT_UNDEFINED
        : dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access,
      dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access,
      srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);
    m_barriers.recordCommands(m_cmd);
      
    if (dstSubresource.aspectMask == srcSubresource.aspectMask) {
      VkImageCopy imageRegion;
      imageRegion.srcSubresource = srcSubresource;
      imageRegion.srcOffset      = srcOffset;
      imageRegion.dstSubresource = dstSubresource;
      imageRegion.dstOffset      = dstOffset;
      imageRegion.extent         = extent;
      
      m_cmd->cmdCopyImage(
        srcImage->handle(), srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        dstImage->handle(), dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        1, &imageRegion);
    } else {
      const VkDeviceSize transferBufferSize = std::max(
        util::computeImageDataSize(dstImage->info().format, extent),
        util::computeImageDataSize(srcImage->info().format, extent));
      
      // TODO optimize away buffer creation
      DxvkBufferCreateInfo tmpBufferInfo;
      tmpBufferInfo.size   = transferBufferSize;
      tmpBufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                           | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      tmpBufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      tmpBufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT
                           | VK_ACCESS_TRANSFER_WRITE_BIT;
      
      Rc<DxvkBuffer> tmpBuffer = m_device->createBuffer(
        tmpBufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      
      DxvkPhysicalBufferSlice tmpSlice = tmpBuffer->slice();
      
      VkBufferImageCopy bufferImageCopy;
      bufferImageCopy.bufferOffset       = tmpSlice.offset();
      bufferImageCopy.bufferRowLength    = 0;
      bufferImageCopy.bufferImageHeight  = 0;
      bufferImageCopy.imageSubresource   = srcSubresource;
      bufferImageCopy.imageOffset        = srcOffset;
      bufferImageCopy.imageExtent        = extent;
      
      m_cmd->cmdCopyImageToBuffer(
        srcImage->handle(),
        srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        tmpSlice.handle(), 1, &bufferImageCopy);
      
      m_barriers.accessBuffer(tmpSlice,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
      m_barriers.recordCommands(m_cmd);
      
      bufferImageCopy.imageSubresource   = dstSubresource;
      bufferImageCopy.imageOffset        = dstOffset;
      
      m_cmd->cmdCopyBufferToImage(tmpSlice.handle(),
        dstImage->handle(),
        dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        1, &bufferImageCopy);
      
      m_barriers.accessBuffer(tmpSlice,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        tmpBuffer->info().stages,
        tmpBuffer->info().access);
      
      m_cmd->trackResource(tmpSlice.resource());
    }
      
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(dstImage);
    m_cmd->trackResource(srcImage);
  }
  
  
  void DxvkContext::copyImageToBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
          VkExtent2D            dstExtent,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            srcExtent) {
    this->spillRenderPass();
    
    auto dstSlice = dstBuffer->subSlice(dstOffset, 0);
    
    VkImageSubresourceRange srcSubresourceRange = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel, 1,
      srcSubresource.baseArrayLayer,
      srcSubresource.layerCount };
    
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access,
      srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);
    m_barriers.recordCommands(m_cmd);
    
    VkBufferImageCopy copyRegion;
    copyRegion.bufferOffset       = dstSlice.offset();
    copyRegion.bufferRowLength    = dstExtent.width;
    copyRegion.bufferImageHeight  = dstExtent.height;
    copyRegion.imageSubresource   = srcSubresource;
    copyRegion.imageOffset        = srcOffset;
    copyRegion.imageExtent        = srcExtent;
    
    m_cmd->cmdCopyImageToBuffer(
      srcImage->handle(),
      srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      dstSlice.handle(),
      1, &copyRegion);
    
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);
    m_barriers.accessBuffer(dstSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstBuffer->info().stages,
      dstBuffer->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(srcImage);
    m_cmd->trackResource(dstSlice.resource());
  }
  
  
  void DxvkContext::dispatch(
          uint32_t x,
          uint32_t y,
          uint32_t z) {
    this->commitComputeState();
    
    if (this->validateComputeState()) {
      m_cmd->cmdDispatch(x, y, z);
      
      this->commitComputeBarriers();
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1);
  }
  
  
  void DxvkContext::dispatchIndirect(
    const DxvkBufferSlice&  buffer) {
    this->commitComputeState();
    
    auto physicalSlice = buffer.physicalSlice();
    
    if (this->validateComputeState()) {
      m_cmd->cmdDispatchIndirect(
        physicalSlice.handle(),
        physicalSlice.offset());
      
      this->commitComputeBarriers();
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1);
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    if (this->validateGraphicsState()) {
      m_cmd->cmdDraw(
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    if (this->validateGraphicsState()) {
      auto physicalSlice = buffer.physicalSlice();
      
      m_cmd->cmdDrawIndirect(
        physicalSlice.handle(),
        physicalSlice.offset(),
        count, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          uint32_t vertexOffset,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    if (this->validateGraphicsState()) {
      m_cmd->cmdDrawIndexed(
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndexedIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    if (this->validateGraphicsState()) {
      auto physicalSlice = buffer.physicalSlice();
      
      m_cmd->cmdDrawIndexedIndirect(
        physicalSlice.handle(),
        physicalSlice.offset(),
        count, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::initImage(
    const Rc<DxvkImage>&           image,
    const VkImageSubresourceRange& subresources) {
    m_barriers.accessImage(image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(image);
  }
  
  
  void DxvkContext::generateMipmaps(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources) {
    if (subresources.levelCount <= 1)
      return;
    
    this->spillRenderPass();

    // The top-most level will only be read. We can
    // discard the contents of all the lower levels
    // since we're going to override them anyway.
    m_barriers.accessImage(image,
      VkImageSubresourceRange {
        subresources.aspectMask, 
        subresources.baseMipLevel, 1,
        subresources.baseArrayLayer,
        subresources.layerCount },
      image->info().layout,
      image->info().stages,
      image->info().access,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);
    
    m_barriers.accessImage(image,
      VkImageSubresourceRange {
        subresources.aspectMask,
        subresources.baseMipLevel + 1,
        subresources.levelCount - 1,
        subresources.baseArrayLayer,
        subresources.layerCount },
      VK_IMAGE_LAYOUT_UNDEFINED,
      image->info().stages,
      image->info().access,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    
    m_barriers.recordCommands(m_cmd);
    
    // Generate each individual mip level with a blit
    for (uint32_t i = 1; i < subresources.levelCount; i++) {
      const uint32_t mip = subresources.baseMipLevel + i;
      
      const VkExtent3D srcExtent = image->mipLevelExtent(mip - 1);
      const VkExtent3D dstExtent = image->mipLevelExtent(mip);
      
      VkImageBlit region;
      region.srcSubresource = VkImageSubresourceLayers {
        subresources.aspectMask, mip - 1,
        subresources.baseArrayLayer,
        subresources.layerCount };
      region.srcOffsets[0]   = VkOffset3D { 0, 0, 0 };
      region.srcOffsets[1].x = srcExtent.width;
      region.srcOffsets[1].y = srcExtent.height;
      region.srcOffsets[1].z = srcExtent.depth;
      
      region.dstSubresource = VkImageSubresourceLayers {
        subresources.aspectMask, mip,
        subresources.baseArrayLayer,
        subresources.layerCount };
      region.dstOffsets[0]   = VkOffset3D { 0, 0, 0 };
      region.dstOffsets[1].x = dstExtent.width;
      region.dstOffsets[1].y = dstExtent.height;
      region.dstOffsets[1].z = dstExtent.depth;
      
      m_cmd->cmdBlitImage(
        image->handle(), image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        image->handle(), image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        1, &region, VK_FILTER_LINEAR);
      
      if (i + 1 < subresources.levelCount) {
        m_barriers.accessImage(image,
          VkImageSubresourceRange {
            subresources.aspectMask, mip, 1,
            subresources.baseArrayLayer,
            subresources.layerCount },
          image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_READ_BIT);
        m_barriers.recordCommands(m_cmd);
      }
    }
    
    // Transform mip levels back into their original layout.
    // The last mip level is still in TRANSFER_DST_OPTIMAL.
    m_barriers.accessImage(image,
      VkImageSubresourceRange {
        subresources.aspectMask,
        subresources.baseMipLevel,
        subresources.levelCount - 1,
        subresources.baseArrayLayer,
        subresources.layerCount },
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    
    m_barriers.accessImage(image,
      VkImageSubresourceRange {
        subresources.aspectMask,
        subresources.baseMipLevel
          + subresources.levelCount - 1, 1,
        subresources.baseArrayLayer,
        subresources.layerCount },
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    
    m_barriers.recordCommands(m_cmd);
  }
  
  
  void DxvkContext::invalidateBuffer(
    const Rc<DxvkBuffer>&           buffer,
    const DxvkPhysicalBufferSlice&  slice) {
    // Allocate new backing resource
    DxvkPhysicalBufferSlice prevSlice = buffer->rename(slice);
    m_cmd->freePhysicalBufferSlice(buffer, prevSlice);
    
    // We also need to update all bindings that the buffer
    // may be bound to either directly or through views.
    const VkBufferUsageFlags usage = buffer->info().usage;
    
    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    
    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    
    if (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
               | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
               | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
               | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
      m_flags.set(DxvkContextFlag::GpDirtyResources,
                  DxvkContextFlag::CpDirtyResources);
  }
  
  
  void DxvkContext::resolveImage(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  format) {
    this->spillRenderPass();
    
    if (format == VK_FORMAT_UNDEFINED)
      format = srcImage->info().format;
    
    if (dstImage->info().format == format
     && srcImage->info().format == format) {
      // Use the built-in Vulkan resolve function if the image
      // formats both match the format of the resolve operation.
      VkImageSubresourceRange dstSubresourceRange = {
        dstSubresources.aspectMask,
        dstSubresources.mipLevel, 1,
        dstSubresources.baseArrayLayer,
        dstSubresources.layerCount };
      
      VkImageSubresourceRange srcSubresourceRange = {
        srcSubresources.aspectMask,
        srcSubresources.mipLevel, 1,
        srcSubresources.baseArrayLayer,
        srcSubresources.layerCount };
      
      // We only support resolving to the entire image
      // area, so we might as well discard its contents
      m_barriers.accessImage(
        dstImage, dstSubresourceRange,
        VK_IMAGE_LAYOUT_UNDEFINED,
        dstImage->info().stages,
        dstImage->info().access,
        dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
      m_barriers.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->info().layout,
        srcImage->info().stages,
        srcImage->info().access,
        srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
      m_barriers.recordCommands(m_cmd);
      
      VkImageResolve imageRegion;
      imageRegion.srcSubresource = srcSubresources;
      imageRegion.srcOffset      = VkOffset3D { 0, 0, 0 };
      imageRegion.dstSubresource = dstSubresources;
      imageRegion.dstOffset      = VkOffset3D { 0, 0, 0 };
      imageRegion.extent         = srcImage->mipLevelExtent(srcSubresources.mipLevel);
      
      m_cmd->cmdResolveImage(
        srcImage->handle(), srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        dstImage->handle(), dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        1, &imageRegion);
    
      m_barriers.accessImage(
        dstImage, dstSubresourceRange,
        dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dstImage->info().layout,
        dstImage->info().stages,
        dstImage->info().access);
      m_barriers.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        srcImage->info().layout,
        srcImage->info().stages,
        srcImage->info().access);
      m_barriers.recordCommands(m_cmd);
    } else {
      // The trick here is to submit an empty render pass which
      // performs the resolve op on properly typed image views.
      const Rc<DxvkMetaResolveFramebuffer> fb =
        new DxvkMetaResolveFramebuffer(m_device->vkd(),
          dstImage, dstSubresources,
          srcImage, srcSubresources, format);
      
      VkRenderPassBeginInfo info;
      info.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      info.pNext            = nullptr;
      info.renderPass       = fb->renderPass();
      info.framebuffer      = fb->framebuffer();
      info.renderArea       = VkRect2D { { 0, 0 }, {
        dstImage->info().extent.width,
        dstImage->info().extent.height } };
      info.clearValueCount  = 0;
      info.pClearValues     = nullptr;
      
      m_cmd->cmdBeginRenderPass(&info, VK_SUBPASS_CONTENTS_INLINE);
      m_cmd->cmdEndRenderPass();
      
      m_cmd->trackResource(fb);
    }
    
    m_cmd->trackResource(srcImage);
    m_cmd->trackResource(dstImage);
  }
  
  
  void DxvkContext::transformImage(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceRange&  dstSubresources,
          VkImageLayout             srcLayout,
          VkImageLayout             dstLayout) {
    this->spillRenderPass();
    
    if (srcLayout != dstLayout) {
      m_barriers.accessImage(
        dstImage, dstSubresources,
        srcLayout,
        dstImage->info().stages,
        dstImage->info().access,
        dstLayout,
        dstImage->info().stages,
        dstImage->info().access);
      m_barriers.recordCommands(m_cmd);
      
      m_cmd->trackResource(dstImage);
    }
  }
  
  
  void DxvkContext::updateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
    const void*                     data) {
    this->spillRenderPass();
    
    // Vulkan specifies that small amounts of data (up to 64kB) can
    // be copied to a buffer directly if the size is a multiple of
    // four. Anything else must be copied through a staging buffer.
    // We'll limit the size to 4kB in order to keep command buffers
    // reasonably small, we do not know how much data apps may upload.
    auto physicalSlice = buffer->subSlice(offset, size);
    
    if ((size <= 4096) && ((size & 0x3) == 0) && ((offset & 0x3) == 0)) {
      m_cmd->cmdUpdateBuffer(
        physicalSlice.handle(),
        physicalSlice.offset(),
        physicalSlice.length(),
        data);
    } else {
      auto slice = m_cmd->stagedAlloc(size);
      std::memcpy(slice.mapPtr, data, size);

      m_cmd->stagedBufferCopy(
        physicalSlice.handle(),
        physicalSlice.offset(),
        physicalSlice.length(),
        slice);
    }

    m_barriers.accessBuffer(
      physicalSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);
    m_barriers.recordCommands(m_cmd);

    m_cmd->trackResource(buffer->resource());
  }
  
  
  void DxvkContext::updateImage(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                imageOffset,
          VkExtent3D                imageExtent,
    const void*                     data,
          VkDeviceSize              pitchPerRow,
          VkDeviceSize              pitchPerLayer) {
    this->spillRenderPass();
    
    // Upload data through a staging buffer. Special care needs to
    // be taken when dealing with compressed image formats: Rather
    // than copying pixels, we'll be copying blocks of pixels.
    const DxvkFormatInfo* formatInfo = image->formatInfo();
    
    // Align image extent to a full block. This is necessary in
    // case the image size is not a multiple of the block size.
    VkExtent3D elementCount = util::computeBlockCount(
      imageExtent, formatInfo->blockSize);
    elementCount.depth *= subresources.layerCount;
    
    // Allocate staging buffer memory for the image data. The
    // pixels or blocks will be tightly packed within the buffer.
    const DxvkStagingBufferSlice slice = m_cmd->stagedAlloc(
      formatInfo->elementSize * util::flattenImageExtent(elementCount));
    
    auto dstData = reinterpret_cast<char*>(slice.mapPtr);
    auto srcData = reinterpret_cast<const char*>(data);
    
    util::packImageData(dstData, srcData,
      elementCount, formatInfo->elementSize,
      pitchPerRow, pitchPerLayer);
    
    // Prepare the image layout. If the given extent covers
    // the entire image, we may discard its previous contents.
    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = subresources.aspectMask;
    subresourceRange.baseMipLevel   = subresources.mipLevel;
    subresourceRange.levelCount     = 1;
    subresourceRange.baseArrayLayer = subresources.baseArrayLayer;
    subresourceRange.layerCount     = subresources.layerCount;
    
    m_barriers.accessImage(
      image, subresourceRange,
      image->mipLevelExtent(subresources.mipLevel) == imageExtent
        ? VK_IMAGE_LAYOUT_UNDEFINED
        : image->info().layout,
      image->info().stages,
      image->info().access,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
    // Copy contents of the staging buffer into the image.
    // Since our source data is tightly packed, we do not
    // need to specify any strides.
    VkBufferImageCopy region;
    region.bufferOffset       = slice.offset;
    region.bufferRowLength    = 0;
    region.bufferImageHeight  = 0;
    region.imageSubresource   = subresources;
    region.imageOffset        = imageOffset;
    region.imageExtent        = imageExtent;
    
    m_cmd->stagedBufferImageCopy(image->handle(),
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      region, slice);
    
    // Transition image back into its optimal layout
    m_barriers.accessImage(
      image, subresourceRange,
      image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(image);
  }
  
  
  void DxvkContext::setViewports(
          uint32_t            viewportCount,
    const VkViewport*         viewports,
    const VkRect2D*           scissorRects) {
    if (m_state.gp.state.rsViewportCount != viewportCount) {
      m_state.gp.state.rsViewportCount = viewportCount;
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
    
    for (uint32_t i = 0; i < viewportCount; i++) {
      m_state.vp.viewports[i]    = viewports[i];
      m_state.vp.scissorRects[i] = scissorRects[i];
      
      // Vulkan viewports are not allowed to have a width or
      // height of zero, so we fall back to a dummy viewport.
      if (viewports[i].width == 0.0f || viewports[i].height == 0.0f) {
        m_state.vp.viewports[i] = VkViewport {
          0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
      }
    }
    
    m_cmd->cmdSetViewport(0, viewportCount, m_state.vp.viewports.data());
    m_cmd->cmdSetScissor (0, viewportCount, m_state.vp.scissorRects.data());
  }
  
  
  void DxvkContext::setBlendConstants(
    const DxvkBlendConstants&   blendConstants) {
    m_state.om.blendConstants = blendConstants;
    m_cmd->cmdSetBlendConstants(&blendConstants.r);
  }
  
  
  void DxvkContext::setStencilReference(
    const uint32_t            reference) {
    m_state.om.stencilReference = reference;
    
    m_cmd->cmdSetStencilReference(
      VK_STENCIL_FRONT_AND_BACK,
      reference);
  }
  
  
  void DxvkContext::setInputAssemblyState(const DxvkInputAssemblyState& ia) {
    m_state.gp.state.iaPrimitiveTopology = ia.primitiveTopology;
    m_state.gp.state.iaPrimitiveRestart  = ia.primitiveRestart;
    m_state.gp.state.iaPatchVertexCount  = ia.patchVertexCount;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setInputLayout(
          uint32_t             attributeCount,
    const DxvkVertexAttribute* attributes,
          uint32_t             bindingCount,
    const DxvkVertexBinding*   bindings) {
    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers);
    
    for (uint32_t i = 0; i < attributeCount; i++) {
      m_state.gp.state.ilAttributes[i].location = attributes[i].location;
      m_state.gp.state.ilAttributes[i].binding  = attributes[i].binding;
      m_state.gp.state.ilAttributes[i].format   = attributes[i].format;
      m_state.gp.state.ilAttributes[i].offset   = attributes[i].offset;
    }
    
    for (uint32_t i = attributeCount; i < m_state.gp.state.ilAttributeCount; i++)
      m_state.gp.state.ilAttributes[i] = VkVertexInputAttributeDescription();
    
    for (uint32_t i = 0; i < bindingCount; i++) {
      m_state.gp.state.ilBindings[i].binding    = bindings[i].binding;
      m_state.gp.state.ilBindings[i].inputRate  = bindings[i].inputRate;
      m_state.gp.state.ilDivisors[i]            = bindings[i].fetchRate;
    }
    
    for (uint32_t i = bindingCount; i < m_state.gp.state.ilBindingCount; i++)
      m_state.gp.state.ilBindings[i] = VkVertexInputBindingDescription();
    
    m_state.gp.state.ilAttributeCount = attributeCount;
    m_state.gp.state.ilBindingCount   = bindingCount;
  }
  
  
  void DxvkContext::setRasterizerState(const DxvkRasterizerState& rs) {
    m_state.gp.state.rsEnableDepthClamp  = rs.enableDepthClamp;
    m_state.gp.state.rsEnableDiscard     = rs.enableDiscard;
    m_state.gp.state.rsPolygonMode       = rs.polygonMode;
    m_state.gp.state.rsCullMode          = rs.cullMode;
    m_state.gp.state.rsFrontFace         = rs.frontFace;
    m_state.gp.state.rsDepthBiasEnable   = rs.depthBiasEnable;
    m_state.gp.state.rsDepthBiasConstant = rs.depthBiasConstant;
    m_state.gp.state.rsDepthBiasClamp    = rs.depthBiasClamp;
    m_state.gp.state.rsDepthBiasSlope    = rs.depthBiasSlope;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setMultisampleState(const DxvkMultisampleState& ms) {
    m_state.gp.state.msSampleMask            = ms.sampleMask;
    m_state.gp.state.msEnableAlphaToCoverage = ms.enableAlphaToCoverage;
    m_state.gp.state.msEnableAlphaToOne      = ms.enableAlphaToOne;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setDepthStencilState(const DxvkDepthStencilState& ds) {
    m_state.gp.state.dsEnableDepthTest   = ds.enableDepthTest;
    m_state.gp.state.dsEnableDepthWrite  = ds.enableDepthWrite;
    m_state.gp.state.dsEnableDepthBounds = ds.enableDepthBounds;
    m_state.gp.state.dsEnableStencilTest = ds.enableStencilTest;
    m_state.gp.state.dsDepthCompareOp    = ds.depthCompareOp;
    m_state.gp.state.dsStencilOpFront    = ds.stencilOpFront;
    m_state.gp.state.dsStencilOpBack     = ds.stencilOpBack;
    m_state.gp.state.dsDepthBoundsMin    = ds.depthBoundsMin;
    m_state.gp.state.dsDepthBoundsMax    = ds.depthBoundsMax;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setLogicOpState(const DxvkLogicOpState& lo) {
    m_state.gp.state.omEnableLogicOp = lo.enableLogicOp;
    m_state.gp.state.omLogicOp       = lo.logicOp;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setBlendMode(
          uint32_t            attachment,
    const DxvkBlendMode&      blendMode) {
    m_state.gp.state.omBlendAttachments[attachment].blendEnable         = blendMode.enableBlending;
    m_state.gp.state.omBlendAttachments[attachment].srcColorBlendFactor = blendMode.colorSrcFactor;
    m_state.gp.state.omBlendAttachments[attachment].dstColorBlendFactor = blendMode.colorDstFactor;
    m_state.gp.state.omBlendAttachments[attachment].colorBlendOp        = blendMode.colorBlendOp;
    m_state.gp.state.omBlendAttachments[attachment].srcAlphaBlendFactor = blendMode.alphaSrcFactor;
    m_state.gp.state.omBlendAttachments[attachment].dstAlphaBlendFactor = blendMode.alphaDstFactor;
    m_state.gp.state.omBlendAttachments[attachment].alphaBlendOp        = blendMode.alphaBlendOp;
    m_state.gp.state.omBlendAttachments[attachment].colorWriteMask      = blendMode.writeMask;
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::signalEvent(const DxvkEventRevision& event) {
    m_cmd->trackEvent(event);
  }
  
  
  void DxvkContext::writeTimestamp(const DxvkQueryRevision& query) {
    DxvkQueryHandle handle = this->allocQuery(query);
    
    m_cmd->cmdWriteTimestamp(
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      handle.queryPool, handle.queryId);
    
    query.query->endRecording(query.revision);
  }
  
  
  void DxvkContext::startRenderPass() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound)
     && (m_state.om.framebuffer != nullptr)) {
      m_flags.set(DxvkContextFlag::GpRenderPassBound);
      m_flags.clr(DxvkContextFlag::GpClearRenderTargets);
      
      this->renderPassBindFramebuffer(
        m_state.om.framebuffer,
        m_state.om.renderPassOps,
        m_state.om.clearValues.size(),
        m_state.om.clearValues.data());
      
      // Don't discard image contents if we have
      // to spill the current render pass
      this->resetRenderPassOps(
        m_state.om.renderTargets,
        m_state.om.renderPassOps);
      
      this->beginActiveQueries();
    }
  }
  
  
  void DxvkContext::spillRenderPass() {
    if (m_flags.test(DxvkContextFlag::GpClearRenderTargets))
      this->startRenderPass();
    
    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      m_flags.clr(DxvkContextFlag::GpRenderPassBound);
      this->endActiveQueries();
      this->renderPassUnbindFramebuffer();
    }
  }
  
  
  void DxvkContext::renderPassBindFramebuffer(
    const Rc<DxvkFramebuffer>&  framebuffer,
    const DxvkRenderPassOps&    ops,
          uint32_t              clearValueCount,
    const VkClearValue*         clearValues) {
    const DxvkFramebufferSize fbSize = framebuffer->size();
    
    VkRect2D renderArea;
    renderArea.offset = VkOffset2D { 0, 0 };
    renderArea.extent = VkExtent2D { fbSize.width, fbSize.height };
    
    VkRenderPassBeginInfo info;
    info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext                = nullptr;
    info.renderPass           = framebuffer->getRenderPassHandle(ops);
    info.framebuffer          = framebuffer->handle();
    info.renderArea           = renderArea;
    info.clearValueCount      = clearValueCount;
    info.pClearValues         = clearValues;
    
    m_cmd->cmdBeginRenderPass(&info,
      VK_SUBPASS_CONTENTS_INLINE);
    m_cmd->trackResource(framebuffer);
    m_cmd->addStatCtr(DxvkStatCounter::CmdRenderPassCount, 1);
  }
  
  
  void DxvkContext::renderPassUnbindFramebuffer() {
    m_cmd->cmdEndRenderPass();
  }
  
  
  void DxvkContext::resetRenderPassOps(
    const DxvkRenderTargets&    renderTargets,
          DxvkRenderPassOps&    renderPassOps) {
    renderPassOps.depthOps = renderTargets.depth.view != nullptr
      ? DxvkDepthAttachmentOps {
          VK_ATTACHMENT_LOAD_OP_LOAD,
          VK_ATTACHMENT_LOAD_OP_LOAD,
          renderTargets.depth.view->imageInfo().layout,
          VK_ATTACHMENT_STORE_OP_STORE,
          VK_ATTACHMENT_STORE_OP_STORE,
          renderTargets.depth.view->imageInfo().layout }
      : DxvkDepthAttachmentOps { };
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      renderPassOps.colorOps[i] = renderTargets.color[i].view != nullptr
        ? DxvkColorAttachmentOps {
            VK_ATTACHMENT_LOAD_OP_LOAD,
            renderTargets.color[i].view->imageInfo().layout,
            VK_ATTACHMENT_STORE_OP_STORE,
            renderTargets.color[i].view->imageInfo().layout }
        : DxvkColorAttachmentOps { };
    }
    
    // TODO provide a sane alternative for this
    if (renderPassOps.colorOps[0].loadLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      renderPassOps.colorOps[0].loadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      renderPassOps.colorOps[0].loadLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }
  
  
  void DxvkContext::unbindComputePipeline() {
    m_flags.set(
      DxvkContextFlag::CpDirtyPipeline,
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::CpDirtyResources);
    
    m_cpActivePipeline = VK_NULL_HANDLE;
  }
  
  
  void DxvkContext::updateComputePipeline() {
    if (m_flags.test(DxvkContextFlag::CpDirtyPipeline)) {
      m_flags.clr(DxvkContextFlag::CpDirtyPipeline);
      
      m_state.cp.state.bsBindingState.clear();
      m_state.cp.pipeline = m_pipeMgr->createComputePipeline(m_state.cp.cs.shader);
      
      if (m_state.cp.pipeline != nullptr)
        m_cmd->trackResource(m_state.cp.pipeline);
    }
  }
  
  
  void DxvkContext::updateComputePipelineState() {
    if (m_flags.test(DxvkContextFlag::CpDirtyPipelineState)) {
      m_flags.clr(DxvkContextFlag::CpDirtyPipelineState);
      
      m_cpActivePipeline = m_state.cp.pipeline != nullptr
        ? m_state.cp.pipeline->getPipelineHandle(m_state.cp.state, m_cmd->statCounters())
        : VK_NULL_HANDLE;
      
      if (m_cpActivePipeline != VK_NULL_HANDLE) {
        m_cmd->cmdBindPipeline(
          VK_PIPELINE_BIND_POINT_COMPUTE,
          m_cpActivePipeline);
      }
    }
  }
  
  
  void DxvkContext::updateGraphicsPipeline() {
    if (m_flags.test(DxvkContextFlag::GpDirtyPipeline)) {
      m_flags.clr(DxvkContextFlag::GpDirtyPipeline);
      
      m_state.gp.state.bsBindingState.clear();
      m_state.gp.pipeline = m_pipeMgr->createGraphicsPipeline(
        m_state.gp.vs.shader,
        m_state.gp.tcs.shader, m_state.gp.tes.shader,
        m_state.gp.gs.shader, m_state.gp.fs.shader);
      
      if (m_state.gp.pipeline != nullptr)
        m_cmd->trackResource(m_state.gp.pipeline);
    }
  }
  
  
  void DxvkContext::updateGraphicsPipelineState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyPipelineState)) {
      m_flags.clr(DxvkContextFlag::GpDirtyPipelineState);
      
      for (uint32_t i = 0; i < m_state.gp.state.ilBindingCount; i++) {
        const uint32_t binding = m_state.gp.state.ilBindings[i].binding;
        
        m_state.gp.state.ilBindings[i].stride
          = (m_state.vi.bindingMask & (1u << binding)) != 0
            ? m_state.vi.vertexStrides[binding]
            : 0;
      }
      
      for (uint32_t i = m_state.gp.state.ilBindingCount; i < MaxNumVertexBindings; i++)
        m_state.gp.state.ilBindings[i].stride = 0;
      
      m_gpActivePipeline = m_state.gp.pipeline != nullptr && m_state.om.framebuffer != nullptr
        ? m_state.gp.pipeline->getPipelineHandle(m_state.gp.state,
            m_state.om.framebuffer->getRenderPass(), m_cmd->statCounters())
        : VK_NULL_HANDLE;
      
      if (m_gpActivePipeline != VK_NULL_HANDLE) {
        m_cmd->cmdBindPipeline(
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          m_gpActivePipeline);
        
        m_cmd->cmdSetViewport(0, m_state.gp.state.rsViewportCount, m_state.vp.viewports.data());
        m_cmd->cmdSetScissor (0, m_state.gp.state.rsViewportCount, m_state.vp.scissorRects.data());
        
        m_cmd->cmdSetBlendConstants(
          &m_state.om.blendConstants.r);
        
        m_cmd->cmdSetStencilReference(
          VK_STENCIL_FRONT_AND_BACK,
          m_state.om.stencilReference);
      }
    }
  }
  
  
  void DxvkContext::updateComputeShaderResources() {
    if (m_flags.test(DxvkContextFlag::CpDirtyResources)) {
      if (m_state.cp.pipeline != nullptr) {
        this->updateShaderResources(
          VK_PIPELINE_BIND_POINT_COMPUTE,
          m_state.cp.pipeline->layout());
      }
    }
  }
  
  
  void DxvkContext::updateComputeShaderDescriptors() {
    if (m_flags.test(DxvkContextFlag::CpDirtyResources)) {
      m_flags.clr(DxvkContextFlag::CpDirtyResources);
      
      if (m_state.cp.pipeline != nullptr) {
        this->updateShaderDescriptors(
          VK_PIPELINE_BIND_POINT_COMPUTE,
          m_state.cp.state.bsBindingState,
          m_state.cp.pipeline->layout());
      }
    }
  }
  
  
  void DxvkContext::updateGraphicsShaderResources() {
    if (m_flags.test(DxvkContextFlag::GpDirtyResources)) {
      if (m_state.gp.pipeline != nullptr) {
        this->updateShaderResources(
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          m_state.gp.pipeline->layout());
      }
    }
  }
  
  
  void DxvkContext::updateGraphicsShaderDescriptors() {
    if (m_flags.test(DxvkContextFlag::GpDirtyResources)) {
      m_flags.clr(DxvkContextFlag::GpDirtyResources);
      
      if (m_state.gp.pipeline != nullptr) {
        this->updateShaderDescriptors(
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          m_state.gp.state.bsBindingState,
          m_state.gp.pipeline->layout());
      }
    }
  }
  
  
  void DxvkContext::updateShaderResources(
          VkPipelineBindPoint     bindPoint,
    const Rc<DxvkPipelineLayout>& layout) {
    DxvkBindingState& bindingState =
      bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
        ? m_state.gp.state.bsBindingState
        : m_state.cp.state.bsBindingState;
    
    bool updatePipelineState = false;
    
    DxvkAttachment depthAttachment;
    
    if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS && m_state.om.framebuffer != nullptr)
      depthAttachment = m_state.om.framebuffer->getDepthTarget();
    
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      const auto& binding = layout->binding(i);
      const auto& res     = m_rc[binding.slot];
      
      switch (binding.type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
          if (res.sampler != nullptr) {
            updatePipelineState |= bindingState.setBound(i);
            
            m_descInfos[i].image.sampler     = res.sampler->handle();
            m_descInfos[i].image.imageView   = VK_NULL_HANDLE;
            m_descInfos[i].image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            m_cmd->trackResource(res.sampler);
          } else {
            updatePipelineState |= bindingState.setUnbound(i);
            m_descInfos[i].image = m_device->dummySamplerDescriptor();
          } break;
        
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          if (res.imageView != nullptr && res.imageView->handle(binding.view) != VK_NULL_HANDLE) {
            updatePipelineState |= bindingState.setBound(i);
            
            m_descInfos[i].image.sampler     = VK_NULL_HANDLE;
            m_descInfos[i].image.imageView   = res.imageView->handle(binding.view);
            m_descInfos[i].image.imageLayout = res.imageView->imageInfo().layout;
            
            if (depthAttachment.view != nullptr
             && depthAttachment.view->image() == res.imageView->image())
              m_descInfos[i].image.imageLayout = depthAttachment.layout;
            
            m_cmd->trackResource(res.imageView);
            m_cmd->trackResource(res.imageView->image());
          } else {
            updatePipelineState |= bindingState.setUnbound(i);
            m_descInfos[i].image = m_device->dummyImageViewDescriptor(binding.view);
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          if (res.bufferView != nullptr) {
            updatePipelineState |= bindingState.setBound(i);
            
            res.bufferView->updateView();
            m_descInfos[i].texelBuffer = res.bufferView->handle();
            
            m_cmd->trackResource(res.bufferView->viewResource());
            m_cmd->trackResource(res.bufferView->bufferResource());
          } else {
            updatePipelineState |= bindingState.setUnbound(i);
            m_descInfos[i].texelBuffer = m_device->dummyBufferViewDescriptor();
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          if (res.bufferSlice.defined()) {
            updatePipelineState |= bindingState.setBound(i);
            
            auto physicalSlice = res.bufferSlice.physicalSlice();
            m_descInfos[i].buffer.buffer = physicalSlice.handle();
            m_descInfos[i].buffer.offset = physicalSlice.offset();
            m_descInfos[i].buffer.range  = physicalSlice.length();
            
            m_cmd->trackResource(physicalSlice.resource());
          } else {
            updatePipelineState |= bindingState.setUnbound(i);
            m_descInfos[i].buffer = m_device->dummyBufferDescriptor();
          } break;
        
        default:
          Logger::err(str::format("DxvkContext: Unhandled descriptor type: ", binding.type));
      }
    }
    
    if (updatePipelineState) {
      m_flags.set(bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
        ? DxvkContextFlag::GpDirtyPipelineState
        : DxvkContextFlag::CpDirtyPipelineState);
    }
  }
  
  
  void DxvkContext::updateShaderDescriptors(
          VkPipelineBindPoint     bindPoint,
    const DxvkBindingState&       bindingState,
    const Rc<DxvkPipelineLayout>& layout) {
    if (layout->bindingCount() != 0) {
      const VkDescriptorSet dset =
        m_cmd->allocateDescriptorSet(
          layout->descriptorSetLayout());
      
      m_cmd->updateDescriptorSetWithTemplate(
        dset, layout->descriptorTemplate(),
        m_descInfos.data());
      
      m_cmd->cmdBindDescriptorSet(bindPoint,
        layout->pipelineLayout(), dset);
    }
  }
  
  
  void DxvkContext::updateFramebuffer() {
    if (m_flags.test(DxvkContextFlag::GpDirtyFramebuffer)) {
      m_flags.clr(DxvkContextFlag::GpDirtyFramebuffer);
      
      this->spillRenderPass();
      
      auto fb = m_device->createFramebuffer(m_state.om.renderTargets);
      
      m_state.gp.state.msSampleCount = fb->getSampleCount();
      m_state.om.framebuffer = fb;
      
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
  }
  
  
  void DxvkContext::updateIndexBufferBinding() {
    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer)) {
      m_flags.clr(DxvkContextFlag::GpDirtyIndexBuffer);
      
      if (m_state.vi.indexBuffer.defined()) {
        auto physicalSlice = m_state.vi.indexBuffer.physicalSlice();
        
        m_cmd->cmdBindIndexBuffer(
          physicalSlice.handle(),
          physicalSlice.offset(),
          m_state.vi.indexType);
        m_cmd->trackResource(
          physicalSlice.resource());
      } else {
        m_cmd->cmdBindIndexBuffer(
          m_device->dummyBufferHandle(),
          0, VK_INDEX_TYPE_UINT32);
      }
    }
  }
  
  
  void DxvkContext::updateVertexBufferBindings() {
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers)) {
      m_flags.clr(DxvkContextFlag::GpDirtyVertexBuffers);
      
      std::array<VkBuffer,     MaxNumVertexBindings> buffers;
      std::array<VkDeviceSize, MaxNumVertexBindings> offsets;
      
      // Set buffer handles and offsets for active bindings
      uint32_t bindingCount = 0;
      uint32_t bindingMask  = 0;
      
      for (uint32_t i = 0; i < m_state.gp.state.ilBindingCount; i++) {
        const uint32_t binding = m_state.gp.state.ilBindings[i].binding;
        bindingCount = std::max(bindingCount, binding + 1);
        
        if (m_state.vi.vertexBuffers[binding].defined()) {
          auto vbo = m_state.vi.vertexBuffers[binding].physicalSlice();
          
          buffers[binding] = vbo.handle();
          offsets[binding] = vbo.offset();
          
          bindingMask |= 1u << binding;
          
          m_cmd->trackResource(vbo.resource());
        }
      }
      
      // Bind a dummy buffer to the remaining bindings
      uint32_t bindingsUsed = (1u << bindingCount) - 1u;
      uint32_t bindingsSet  = bindingMask;
      
      while (bindingsSet != bindingsUsed) {
        uint32_t binding = bit::tzcnt(~bindingsSet);
        
        buffers[binding] = m_device->dummyBufferHandle();
        offsets[binding] = 0;
        
        bindingsSet |= 1u << binding;
      }
      
      // Bind all vertex buffers at once
      if (bindingCount != 0) {
        m_cmd->cmdBindVertexBuffers(0, bindingCount,
          buffers.data(), offsets.data());
      }
      
      // If the set of active bindings has changed, we'll
      // need to adjust the strides of the inactive ones
      // and compile a new pipeline
      if (m_state.vi.bindingMask != bindingMask) {
        m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
        m_state.vi.bindingMask = bindingMask;
      }
    }
  }
  
  
  bool DxvkContext::validateComputeState() {
    return m_cpActivePipeline != VK_NULL_HANDLE;
  }
  
  
  bool DxvkContext::validateGraphicsState() {
    if (m_gpActivePipeline == VK_NULL_HANDLE)
      return false;
    
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound))
      return false;
    
    return true;
  }
  
  
  void DxvkContext::commitComputeState() {
    this->spillRenderPass();
    this->updateComputePipeline();
    this->updateComputeShaderResources();
    this->updateComputePipelineState();
    this->updateComputeShaderDescriptors();
  }
  
  
  void DxvkContext::commitGraphicsState() {
    this->updateFramebuffer();
    this->startRenderPass();
    this->updateGraphicsPipeline();
    this->updateIndexBufferBinding();
    this->updateVertexBufferBindings();
    this->updateGraphicsShaderResources();
    this->updateGraphicsPipelineState();
    this->updateGraphicsShaderDescriptors();
  }
  
  
  void DxvkContext::commitComputeBarriers() {
    // TODO optimize. Each pipeline layout should
    // hold a list of resource that can be written.
    // TODO generalize so that this can be used for
    // graphics pipelines as well
    auto layout = m_state.cp.pipeline->layout();
    
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      if (m_state.cp.state.bsBindingState.isBound(i)) {
        const DxvkDescriptorSlot binding = layout->binding(i);
        const DxvkShaderResourceSlot& slot = m_rc[binding.slot];
        
        if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
          m_barriers.accessBuffer(
            slot.bufferSlice.physicalSlice(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | 
            VK_ACCESS_SHADER_WRITE_BIT,
            slot.bufferSlice.bufferInfo().stages,
            slot.bufferSlice.bufferInfo().access);
        } else if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
          m_barriers.accessBuffer(
            slot.bufferView->physicalSlice(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | 
            VK_ACCESS_SHADER_WRITE_BIT,
            slot.bufferView->bufferInfo().stages,
            slot.bufferView->bufferInfo().access);
        } else if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
          m_barriers.accessImage(
            slot.imageView->image(),
            slot.imageView->subresources(),
            slot.imageView->imageInfo().layout,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | 
            VK_ACCESS_SHADER_WRITE_BIT,
            slot.imageView->imageInfo().layout,
            slot.imageView->imageInfo().stages,
            slot.imageView->imageInfo().access);
        }
      }
    }
    
    m_barriers.recordCommands(m_cmd);
  }
  
  
    
  DxvkQueryHandle DxvkContext::allocQuery(const DxvkQueryRevision& query) {
    const VkQueryType queryType = query.query->type();
    
    DxvkQueryHandle queryHandle = DxvkQueryHandle();
    Rc<DxvkQueryPool> queryPool = m_queryPools[queryType];
    
    if (queryPool != nullptr)
      queryHandle = queryPool->allocQuery(query);
    
    if (queryHandle.queryPool == VK_NULL_HANDLE) {
      if (queryPool != nullptr)
        this->trackQueryPool(queryPool);
      
      m_queryPools[queryType] = m_device->createQueryPool(queryType, MaxNumQueryCountPerPool);
      queryPool = m_queryPools[queryType];
      
      queryPool->reset(m_cmd);
      queryHandle = queryPool->allocQuery(query);
    }
    
    return queryHandle;
  }
  
  
  void DxvkContext::trackQueryPool(const Rc<DxvkQueryPool>& pool) {
    if (pool != nullptr) {
      DxvkQueryRange range = pool->getActiveQueryRange();
      
      if (range.queryCount > 0)
        m_cmd->trackQueryRange(std::move(range));
    }
  }
  
  
  void DxvkContext::beginActiveQueries() {
    for (const DxvkQueryRevision& query : m_activeQueries) {
      DxvkQueryHandle handle = this->allocQuery(query);
      
      m_cmd->cmdBeginQuery(
        handle.queryPool,
        handle.queryId,
        handle.flags);
    }
  }
  
  
  void DxvkContext::endActiveQueries() {
    for (const DxvkQueryRevision& query : m_activeQueries) {
      DxvkQueryHandle handle = query.query->getHandle();
      
      m_cmd->cmdEndQuery(
        handle.queryPool,
        handle.queryId);
    }
  }
  
  
  void DxvkContext::insertActiveQuery(const DxvkQueryRevision& query) {
    m_activeQueries.push_back(query);
  }
  
  
  void DxvkContext::eraseActiveQuery(const DxvkQueryRevision& query) {
    for (auto i = m_activeQueries.begin(); i != m_activeQueries.end(); i++) {
      if (i->query == query.query && i->revision == query.revision) {
        m_activeQueries.erase(i);
        return;
      }
    }
  }
  
}