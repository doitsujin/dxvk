#include <cstring>

#include "dxvk_device.h"
#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<DxvkDevice>& device)
  : m_device(device) {
    for (uint32_t i = 0; i < m_descWrites.size(); i++) {
      m_descWrites[i].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      m_descWrites[i].pNext            = nullptr;
      m_descWrites[i].dstSet           = VK_NULL_HANDLE;
      m_descWrites[i].dstBinding       = i;
      m_descWrites[i].dstArrayElement  = 0;
      m_descWrites[i].descriptorCount  = 1;
      m_descWrites[i].descriptorType   = VkDescriptorType(0);
      m_descWrites[i].pImageInfo       = &m_descInfos[i].image;
      m_descWrites[i].pBufferInfo      = &m_descInfos[i].buffer;
      m_descWrites[i].pTexelBufferView = &m_descInfos[i].texelBuffer;
    }
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
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyResources,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::CpDirtyPipeline,
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::CpDirtyResources);
    
    // Restart queries that were active during
    // the last command buffer submission.
    this->beginActiveQueries();
  }
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording() {
    this->renderPassEnd();
    this->endActiveQueries();
    
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_OCCLUSION]);
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_PIPELINE_STATISTICS]);
    this->trackQueryPool(m_queryPools[VK_QUERY_TYPE_TIMESTAMP]);
    
    m_cmd->endRecording();
    return std::exchange(m_cmd, nullptr);
  }
  
  
  void DxvkContext::beginQuery(const DxvkQueryRevision& query) {
    DxvkQueryHandle handle = this->allocQuery(query);
    
    m_cmd->cmdBeginQuery(
      handle.queryPool,
      handle.queryId,
      handle.flags);
    
    query.query->beginRecording(query.revision);
    this->insertActiveQuery(query);
  }
  
  
  void DxvkContext::endQuery(const DxvkQueryRevision& query) {
    DxvkQueryHandle handle = query.query->getHandle();
    
    m_cmd->cmdEndQuery(
      handle.queryPool,
      handle.queryId);
    
    query.query->endRecording(query.revision);
    this->eraseActiveQuery(query);
  }
  
    
  void DxvkContext::bindFramebuffer(const Rc<DxvkFramebuffer>& fb) {
    if (m_state.om.framebuffer != fb) {
      this->renderPassEnd();
      m_state.om.framebuffer = fb;
      
      if (fb != nullptr) {
        m_state.gp.state.msSampleCount = fb->sampleCount();
        m_state.gp.state.omRenderPass  = fb->renderPass();
        m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
      }
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
  
  
  void DxvkContext::bindResourceTexelBuffer(
          uint32_t              slot,
    const Rc<DxvkBufferView>&   bufferView) {
    if (m_rc[slot].bufferView != bufferView) {
      m_rc[slot].sampler     = nullptr;
      m_rc[slot].imageView   = nullptr;
      m_rc[slot].bufferView  = bufferView;
      m_rc[slot].bufferSlice = DxvkBufferSlice();
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
    }
  }
  
  
  void DxvkContext::bindResourceImage(
          uint32_t              slot,
    const Rc<DxvkImageView>&    image) {
    if (m_rc[slot].imageView != image) {
      m_rc[slot].sampler     = nullptr;
      m_rc[slot].imageView   = image;
      m_rc[slot].bufferView  = nullptr;
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
    this->renderPassEnd();
    
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
  
  
  void DxvkContext::clearColorImage(
    const Rc<DxvkImage>&            image,
    const VkClearColorValue&        value,
    const VkImageSubresourceRange&  subresources) {
    this->renderPassEnd();
    
    m_barriers.accessImage(image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED,
      image->info().stages,
      image->info().access,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
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
  
  
  void DxvkContext::clearDepthStencilImage(
    const Rc<DxvkImage>&            image,
    const VkClearDepthStencilValue& value,
    const VkImageSubresourceRange&  subresources) {
    this->renderPassEnd();
    
    m_barriers.accessImage(
      image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED,
      image->info().stages,
      image->info().access,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->cmdClearDepthStencilImage(image->handle(),
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      &value, 1, &subresources);
    
    m_barriers.accessImage(
      image, subresources,
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
    if (numBytes == 0)
      return;

    this->renderPassEnd();

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
    this->renderPassEnd();
    
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &copyRegion);
    
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    this->renderPassEnd();
    
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        srcImage->handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
        srcImage->handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        tmpSlice.handle(), 1, &bufferImageCopy);
      
      m_barriers.accessBuffer(tmpSlice,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
      m_barriers.recordCommands(m_cmd);
      
      bufferImageCopy.imageSubresource   = dstSubresource;
      bufferImageCopy.imageOffset        = dstOffset;
      
      m_cmd->cmdCopyBufferToImage(
        tmpSlice.handle(), dstImage->handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
    this->renderPassEnd();
    
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
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dstSlice.handle(),
      1, &copyRegion);
    
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
    
    if (m_cpActivePipeline != VK_NULL_HANDLE) {
      m_cmd->cmdDispatch(x, y, z);
      
      this->commitComputeBarriers();
    }
  }
  
  
  void DxvkContext::dispatchIndirect(
    const DxvkBufferSlice&  buffer) {
    this->commitComputeState();
    
    auto physicalSlice = buffer.physicalSlice();
    
    if (m_cpActivePipeline != VK_NULL_HANDLE) {
      m_cmd->cmdDispatchIndirect(
        physicalSlice.handle(),
        physicalSlice.offset());
      
      this->commitComputeBarriers();
    }
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    if (m_gpActivePipeline != VK_NULL_HANDLE) {
      m_cmd->cmdDraw(
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
  }
  
  
  void DxvkContext::drawIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    if (m_gpActivePipeline != VK_NULL_HANDLE) {
      auto physicalSlice = buffer.physicalSlice();
      
      m_cmd->cmdDrawIndirect(
        physicalSlice.handle(),
        physicalSlice.offset(),
        count, stride);
    }
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          uint32_t vertexOffset,
          uint32_t firstInstance) {
    this->commitGraphicsState();
    
    if (m_gpActivePipeline != VK_NULL_HANDLE) {
      m_cmd->cmdDrawIndexed(
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
  }
  
  
  void DxvkContext::drawIndexedIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    if (m_gpActivePipeline != VK_NULL_HANDLE) {
      auto physicalSlice = buffer.physicalSlice();
      
      m_cmd->cmdDrawIndexedIndirect(
        physicalSlice.handle(),
        physicalSlice.offset(),
        count, stride);
    }
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
  }
  
  
  void DxvkContext::generateMipmaps(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources) {
    if (subresources.levelCount <= 1)
      return;
    
    this->renderPassEnd();

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
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
        image->handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);
      
      if (i + 1 < subresources.levelCount) {
        m_barriers.accessImage(image,
          VkImageSubresourceRange {
            subresources.aspectMask, mip, 1,
            subresources.baseArrayLayer,
            subresources.layerCount },
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    buffer->rename(slice);
    
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
    const VkImageSubresourceLayers& srcSubresources) {
    this->renderPassEnd();

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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
      srcImage->handle(),
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dstImage->handle(),
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &imageRegion);
    
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);
    m_barriers.accessImage(
      srcImage, srcSubresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);
    m_barriers.recordCommands(m_cmd);
  }
  
  
  void DxvkContext::updateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
    const void*                     data) {
    this->renderPassEnd();
    
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
    this->renderPassEnd();
    
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      region, slice);
    
    // Transition image back into its optimal layout
    m_barriers.accessImage(
      image, subresourceRange,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    }
    
    m_cmd->cmdSetViewport(0, viewportCount, viewports);
    m_cmd->cmdSetScissor (0, viewportCount, scissorRects);
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
    m_flags.clr(
      DxvkContextFlag::GpEmulateInstanceFetchRate);
    
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
      m_state.vi.vertexFetchRates[bindings[i].binding] = bindings[i].fetchRate;
      
      if (bindings[i].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE && bindings[i].fetchRate != 1)
        m_flags.set(DxvkContextFlag::GpEmulateInstanceFetchRate);
    }
    
    for (uint32_t i = bindingCount; i < m_state.gp.state.ilBindingCount; i++)
      m_state.gp.state.ilBindings[i] = VkVertexInputBindingDescription();
    
    m_state.gp.state.ilAttributeCount = attributeCount;
    m_state.gp.state.ilBindingCount   = bindingCount;
    
    if (m_flags.test(DxvkContextFlag::GpEmulateInstanceFetchRate)) {
      static bool errorShown = false;
      
      if (!std::exchange(errorShown, true))
        Logger::warn("Dxvk: GpEmulateInstanceFetchRate not handled yet");
    }
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
    m_state.gp.state.msEnableSampleShading   = ms.enableSampleShading;
    m_state.gp.state.msMinSampleShading      = ms.minSampleShading;
    
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
  
  
  void DxvkContext::renderPassBegin() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound)
     && (m_state.om.framebuffer != nullptr)) {
      m_flags.set(DxvkContextFlag::GpRenderPassBound);
      
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
    }
  }
  
  
  void DxvkContext::updateComputePipeline() {
    if (m_flags.test(DxvkContextFlag::CpDirtyPipeline)) {
      m_flags.clr(DxvkContextFlag::CpDirtyPipeline);
      
      m_state.cp.state.bsBindingState.clear();
      m_state.cp.pipeline = m_device->createComputePipeline(
        m_state.cp.cs.shader);
      
      if (m_state.cp.pipeline != nullptr)
        m_cmd->trackResource(m_state.cp.pipeline);
    }
  }
  
  
  void DxvkContext::updateComputePipelineState() {
    if (m_flags.test(DxvkContextFlag::CpDirtyPipelineState)) {
      m_flags.clr(DxvkContextFlag::CpDirtyPipelineState);
      
      m_cpActivePipeline = m_state.cp.pipeline != nullptr
        ? m_state.cp.pipeline->getPipelineHandle(m_state.cp.state)
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
      m_state.gp.pipeline = m_device->createGraphicsPipeline(
        m_state.gp.vs.shader, m_state.gp.tcs.shader, m_state.gp.tes.shader,
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
      
      m_gpActivePipeline = m_state.gp.pipeline != nullptr
        ? m_state.gp.pipeline->getPipelineHandle(m_state.gp.state)
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
    DxvkBindingState& bs =
      bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
        ? m_state.gp.state.bsBindingState
        : m_state.cp.state.bsBindingState;
    
    bool updatePipelineState = false;
    
    // TODO recreate resource views if the underlying
    // resource was marked as dirty after invalidation
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      const auto& binding = layout->binding(i);
      const auto& res     = m_rc[binding.slot];
      
      switch (binding.type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
          if (res.sampler != nullptr) {
            updatePipelineState |= bs.setBound(i);
            
            m_descInfos[i].image.sampler     = res.sampler->handle();
            m_descInfos[i].image.imageView   = VK_NULL_HANDLE;
            m_descInfos[i].image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            m_cmd->trackResource(res.sampler);
          } else {
            updatePipelineState |= bs.setUnbound(i);
            m_descInfos[i].image = m_device->dummySamplerDescriptor();
          } break;
        
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          if (res.imageView != nullptr && res.imageView->type() == binding.view) {
            updatePipelineState |= bs.setBound(i);
            
            m_descInfos[i].image.sampler     = VK_NULL_HANDLE;
            m_descInfos[i].image.imageView   = res.imageView->handle();
            m_descInfos[i].image.imageLayout = res.imageView->imageInfo().layout;
            
            // TODO try to reduce the runtime overhead of all these comparisons
            if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
              DxvkAttachment depthAttachment = m_state.om.framebuffer->renderTargets().getDepthTarget();
              
              if (depthAttachment.view != nullptr
               && depthAttachment.view->image() == res.imageView->image())
                m_descInfos[i].image.imageLayout = depthAttachment.layout;
            }
            
            m_cmd->trackResource(res.imageView);
            m_cmd->trackResource(res.imageView->image());
          } else {
            updatePipelineState |= bs.setUnbound(i);
            m_descInfos[i].image = m_device->dummyImageViewDescriptor(binding.view);
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          if (res.bufferView != nullptr) {
            updatePipelineState |= bs.setBound(i);
            
            m_descInfos[i].texelBuffer = res.bufferView->handle();
            
            m_cmd->trackResource(res.bufferView);
            m_cmd->trackResource(res.bufferView->resource());
          } else {
            updatePipelineState |= bs.setUnbound(i);
            m_descInfos[i].texelBuffer = m_device->dummyBufferViewDescriptor();
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          if (res.bufferSlice.defined()) {
            updatePipelineState |= bs.setBound(i);
            
            auto physicalSlice = res.bufferSlice.physicalSlice();
            m_descInfos[i].buffer.buffer = physicalSlice.handle();
            m_descInfos[i].buffer.offset = physicalSlice.offset();
            m_descInfos[i].buffer.range  = physicalSlice.length();
            
            m_cmd->trackResource(physicalSlice.resource());
          } else {
            updatePipelineState |= bs.setUnbound(i);
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
    const VkDescriptorSet dset =
      m_cmd->allocateDescriptorSet(
        layout->descriptorSetLayout());
    
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      m_descWrites[i].dstSet         = dset;
      m_descWrites[i].descriptorType = layout->binding(i).type;
    }
    
    m_cmd->updateDescriptorSet(
      layout->bindingCount(), m_descWrites.data());
    m_cmd->cmdBindDescriptorSet(bindPoint,
      layout->pipelineLayout(), dset);
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
      
      uint32_t bindingMask = 0;
      
      for (uint32_t i = 0; i < m_state.gp.state.ilBindingCount; i++) {
        const uint32_t binding = m_state.gp.state.ilBindings[i].binding;
        
        if (m_state.vi.vertexBuffers[binding].defined()) {
          auto vbo = m_state.vi.vertexBuffers[binding].physicalSlice();
          
          const VkBuffer     handle = vbo.handle();
          const VkDeviceSize offset = vbo.offset();
          
          m_cmd->cmdBindVertexBuffers(binding, 1, &handle, &offset);
          m_cmd->trackResource(vbo.resource());
          
          bindingMask |= 1u << binding;
        } else {
          const VkBuffer     handle = m_device->dummyBufferHandle();
          const VkDeviceSize offset = 0;
          
          m_cmd->cmdBindVertexBuffers(binding, 1, &handle, &offset);
        }
      }
      
      if (m_state.vi.bindingMask != bindingMask) {
        m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
        m_state.vi.bindingMask = bindingMask;
      }
    }
  }
  
  
  void DxvkContext::commitComputeState() {
    this->renderPassEnd();
    this->updateComputePipeline();
    this->updateComputeShaderResources();
    this->updateComputePipelineState();
    this->updateComputeShaderDescriptors();
  }
  
  
  void DxvkContext::commitGraphicsState() {
    this->renderPassBegin();
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
            slot.bufferView->slice(),
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
      
      this->resetQueryPool(queryPool);
      queryHandle = queryPool->allocQuery(query);
    }
    
    return queryHandle;
  }
  
  
  void DxvkContext::resetQueryPool(const Rc<DxvkQueryPool>& pool) {
    this->renderPassEnd();
    
    pool->reset(m_cmd);
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