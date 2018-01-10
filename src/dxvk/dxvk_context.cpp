#include <cstring>

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
      DxvkContextFlag::GpDirtyPipelineState,
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
    const DxvkBufferSlice&      buffer,
          VkIndexType           indexType) {
    if (m_state.vi.indexBuffer != buffer
     || m_state.vi.indexType   != indexType) {
      m_state.vi.indexBuffer = buffer;
      m_state.vi.indexType   = indexType;
      
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    }
  }
  
  
  void DxvkContext::bindResourceBuffer(
          uint32_t              slot,
    const DxvkBufferSlice&      buffer) {
    if (m_rc[slot].bufferSlice != buffer) {
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
      m_rc[slot].bufferView = bufferView;
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
    }
  }
  
  
  void DxvkContext::bindResourceImage(
          uint32_t              slot,
    const Rc<DxvkImageView>&    image) {
    if (m_rc[slot].imageView != image) {
      m_rc[slot].imageView = image;
      
      m_flags.set(
        DxvkContextFlag::CpDirtyResources,
        DxvkContextFlag::GpDirtyResources);
    }
  }
  
  
  void DxvkContext::bindResourceSampler(
          uint32_t              slot,
    const Rc<DxvkSampler>&      sampler) {
    if (m_rc[slot].sampler != sampler) {
      m_rc[slot].sampler = sampler;
      
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
          DxvkContextFlag::CpDirtyResources);
      } else {
        m_flags.set(
          DxvkContextFlag::GpDirtyPipeline,
          DxvkContextFlag::GpDirtyResources);
      }
    }
  }
  
  
  void DxvkContext::bindVertexBuffer(
          uint32_t              binding,
    const DxvkBufferSlice&      buffer,
          uint32_t              stride) {
    if (m_state.vi.vertexBuffers[binding] != buffer) {
      m_state.vi.vertexBuffers[binding] = buffer;
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    }
    
    if (m_state.vi.vertexStrides[binding] != stride) {
      m_state.vi.vertexStrides[binding] = stride;
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
  
  
  void DxvkContext::clearDepthStencilImage(
    const Rc<DxvkImage>&            image,
    const VkClearDepthStencilValue& value,
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
    
    m_cmd->cmdClearDepthStencilImage(image->handle(),
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
      this->renderPassEnd();
      
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
      
      m_cmd->trackResource(dstBuffer->resource());
      m_cmd->trackResource(srcBuffer->resource());
    }
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
    
    const VkImageSubresourceRange dstSubresourceRange = {
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
    copyRegion.bufferOffset       = srcOffset;
    copyRegion.bufferRowLength    = srcExtent.width;
    copyRegion.bufferImageHeight  = srcExtent.height;
    copyRegion.imageSubresource   = dstSubresource;
    copyRegion.imageOffset        = dstOffset;
    copyRegion.imageExtent        = dstExtent;
    
    m_cmd->cmdCopyBufferToImage(
      srcBuffer->handle(),
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
    m_barriers.recordCommands(m_cmd);
    
    m_cmd->trackResource(dstImage);
    m_cmd->trackResource(srcBuffer->resource());
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
    
    const VkImageSubresourceRange dstSubresourceRange = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel, 1,
      dstSubresource.baseArrayLayer,
      dstSubresource.layerCount };
    
    const VkImageSubresourceRange srcSubresourceRange = {
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
  
  
  void DxvkContext::dispatch(
          uint32_t x,
          uint32_t y,
          uint32_t z) {
    this->commitComputeState();
    
    m_cmd->cmdDispatch(x, y, z);
    
    this->commitComputeBarriers();
  }
  
  
  void DxvkContext::dispatchIndirect(
    const DxvkBufferSlice&  buffer) {
    this->commitComputeState();
    
    m_cmd->cmdDispatchIndirect(
      buffer.handle(),
      buffer.offset());
    
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
  
  
  void DxvkContext::drawIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    m_cmd->cmdDrawIndirect(
      buffer.handle(),
      buffer.offset(),
      count, stride);
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
  
  
  void DxvkContext::drawIndexedIndirect(
    const DxvkBufferSlice&  buffer,
          uint32_t          count,
          uint32_t          stride) {
    this->commitGraphicsState();
    
    m_cmd->cmdDrawIndexedIndirect(
      buffer.handle(),
      buffer.offset(),
      count, stride);
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
  
  
  void DxvkContext::invalidateBuffer(const Rc<DxvkBuffer>& buffer) {
    // Allocate new backing resource
    buffer->renameResource(
      buffer->allocateResource());
    
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
    VkImageSubresourceRange dstSubresourceRange = {
      dstSubresources.aspectMask,
      dstSubresources.mipLevel, 1,
      dstSubresources.baseArrayLayer,
      dstSubresources.layerCount,
    };
    
    VkImageSubresourceRange srcSubresourceRange = {
      srcSubresources.aspectMask,
      srcSubresources.mipLevel, 1,
      srcSubresources.baseArrayLayer,
      srcSubresources.layerCount,
    };
    
    // We only support resolving to the entire image
    // area, so we might as well discard its contents
    m_barriers.accessImage(
      dstImage, dstSubresourceRange,
      VK_IMAGE_LAYOUT_UNDEFINED, 0, 0,
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
    
    if (size != 0) {
      // Vulkan specifies that small amounts of data (up to 64kB) can
      // be copied to a buffer directly if the size is a multiple of
      // four. Anything else must be copied through a staging buffer.
      if ((size <= 65536) && ((size & 0x3) == 0) && ((offset & 0x3) == 0)) {
        m_cmd->cmdUpdateBuffer(
          buffer->handle(),
          offset, size, data);
      } else {
        auto slice = m_cmd->stagedAlloc(size);
        std::memcpy(slice.mapPtr, data, size);
        
        m_cmd->stagedBufferCopy(
          buffer->handle(),
          offset, size, slice);
      }
      
      m_barriers.accessBuffer(
        buffer, offset, size,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        buffer->info().stages,
        buffer->info().access);
      m_barriers.recordCommands(m_cmd);
      
      m_cmd->trackResource(buffer->resource());
    }
  }
  
  
  void DxvkContext::updateImage(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                imageOffset,
          VkExtent3D                imageExtent,
    const void*                     data,
          VkDeviceSize              pitchPerRow,
          VkDeviceSize              pitchPerLayer) {
    if (subresources.layerCount == 0) {
      Logger::warn("DxvkContext::updateImage: Layer count is zero");
      return;
    }
    
    // Upload data through a staging buffer. Special care needs to
    // be taken when dealing with compressed image formats: Rather
    // than copying pixels, we'll be copying blocks of pixels.
    const DxvkFormatInfo* formatInfo
      = imageFormatInfo(image->info().format);
    
    VkExtent3D elementCount = imageExtent;
    elementCount.depth *= subresources.layerCount;
    
    // Align image extent to a full block. This is necessary in
    // case the image size is not a multiple of the block size.
    elementCount.width  += formatInfo->blockSize.width  - 1;
    elementCount.height += formatInfo->blockSize.height - 1;
    elementCount.depth  += formatInfo->blockSize.depth  - 1;
    
    elementCount.width  /= formatInfo->blockSize.width;
    elementCount.height /= formatInfo->blockSize.height;
    elementCount.depth  /= formatInfo->blockSize.depth;
    
    VkDeviceSize bytesPerRow   = elementCount.width  * formatInfo->elementSize;
    VkDeviceSize bytesPerLayer = elementCount.height * bytesPerRow;
    VkDeviceSize bytesTotal    = elementCount.depth  * bytesPerLayer;
    
    // Allocate staging buffer memory for the image data. The
    // pixels or blocks will be tightly packed within the buffer.
    DxvkStagingBufferSlice slice = m_cmd->stagedAlloc(bytesTotal);
    
    auto dstData = reinterpret_cast<char*>(slice.mapPtr);
    auto srcData = reinterpret_cast<const char*>(data);
    
    // If the application provides tightly packed data as well,
    // we can minimize the number of memcpy calls in order to
    // improve performance.
    bool useDirectCopy = true;
    
    useDirectCopy &= (pitchPerLayer == bytesPerLayer) || (elementCount.depth  == 1);
    useDirectCopy &= (pitchPerRow   == bytesPerRow)   || (elementCount.height == 1);
    
    if (useDirectCopy) {
      std::memcpy(dstData, srcData, bytesTotal);
    } else {
      for (uint32_t i = 0; i < elementCount.depth; i++) {
        for (uint32_t j = 0; j < elementCount.height; j++) {
          std::memcpy(
            dstData + j * bytesPerRow,
            srcData + j * pitchPerRow,
            bytesPerRow);
        }
        
        srcData += pitchPerLayer;
        dstData += bytesPerLayer;
      }
    }
    
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
    if (m_state.vp.viewportCount != viewportCount) {
      m_state.vp.viewportCount = viewportCount;
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
    
    for (uint32_t i = 0; i < viewportCount; i++) {
      m_state.vp.viewports[i]    = viewports[i];
      m_state.vp.scissorRects[i] = scissorRects[i];
    }
    
    this->updateViewports();
  }
  
  
  void DxvkContext::setBlendConstants(
    const float               blendConstants[4]) {
    for (uint32_t i = 0; i < 4; i++)
      m_state.om.blendConstants[i] = blendConstants[i];
    
    this->updateBlendConstants();
  }
  
  
  void DxvkContext::setStencilReference(
    const uint32_t            reference) {
    m_state.om.stencilReference = reference;
    
    this->updateStencilReference();
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
    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers);
    
    m_state.il.numAttributes = attributeCount;
    m_state.il.numBindings   = bindingCount;
    
    for (uint32_t i = 0; i < attributeCount; i++)
      m_state.il.attributes[i] = attributes[i];
    
    for (uint32_t i = 0; i < bindingCount; i++)
      m_state.il.bindings[i] = bindings[i];
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
    m_state.om.blendModes[attachment] = blendMode;
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
        gpState.ilBindings[i].stride     = m_state.vi.vertexStrides[i];
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
      
      gpState.msSampleCount            = m_state.om.framebuffer->sampleCount();
      gpState.msSampleMask             = m_state.ms.sampleMask;
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
          const DxvkBlendMode& mode = m_state.om.blendModes[i];
          
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
      
      this->updateShaderResources(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_state.cp.pipeline->layout());
    }
  }
  
  
  void DxvkContext::updateGraphicsShaderResources() {
    if (m_flags.test(DxvkContextFlag::GpDirtyResources)) {
      m_flags.clr(DxvkContextFlag::GpDirtyResources);
      
      this->updateShaderResources(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_state.gp.pipeline->layout());
    }
  }
  
  
  void DxvkContext::updateShaderResources(
          VkPipelineBindPoint     bindPoint,
    const Rc<DxvkBindingLayout>&  layout) {
    // TODO recreate resource views if the underlying
    // resource was marked as dirty after invalidation
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      const auto& binding = layout->binding(i);
      const auto& res     = m_rc[binding.slot];
      
      switch (binding.type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
          if (res.sampler != nullptr) {
            m_descriptors[i].image.sampler     = res.sampler->handle();
            m_descriptors[i].image.imageView   = VK_NULL_HANDLE;
            m_descriptors[i].image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            m_cmd->trackResource(res.sampler);
          } else {
            Logger::err("DxvkContext: Unbound sampler descriptor");
            m_descriptors[i].image.sampler     = VK_NULL_HANDLE;
            m_descriptors[i].image.imageView   = VK_NULL_HANDLE;
            m_descriptors[i].image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          } break;
        
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          if (res.imageView != nullptr && res.imageView->type() != binding.view) {
            m_descriptors[i].image.sampler     = VK_NULL_HANDLE;
            m_descriptors[i].image.imageView   = res.imageView->handle();
            m_descriptors[i].image.imageLayout = res.imageView->imageInfo().layout;
            
            m_cmd->trackResource(res.imageView);
            m_cmd->trackResource(res.imageView->image());
          } else {
            Logger::err("DxvkContext: Unbound or incompatible image descriptor");
            m_descriptors[i].image.sampler     = VK_NULL_HANDLE;
            m_descriptors[i].image.imageView   = VK_NULL_HANDLE;
            m_descriptors[i].image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          if (res.bufferView != nullptr) {
            m_descriptors[i].texelBuffer = res.bufferView->handle();
            
            m_cmd->trackResource(res.bufferView);
            m_cmd->trackResource(res.bufferView->buffer()->resource());
          } else {
            Logger::err("DxvkContext: Unbound texel buffer");
            m_descriptors[i].texelBuffer = VK_NULL_HANDLE;
          } break;
        
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          if (res.bufferSlice.handle() != VK_NULL_HANDLE) {
            m_descriptors[i].buffer = res.bufferSlice.descriptorInfo();
            m_cmd->trackResource(res.bufferSlice.resource());
          } else {
            Logger::err("DxvkContext: Unbound buffer");
            m_descriptors[i].buffer.buffer = VK_NULL_HANDLE;
            m_descriptors[i].buffer.offset = 0;
            m_descriptors[i].buffer.range  = 0;
          } break;
        
        default:
          Logger::err(str::format("DxvkContext: Unhandled descriptor type: ", binding.type));
      }
    }
    
    m_cmd->bindResourceDescriptors(
      bindPoint,
      layout->pipelineLayout(),
      layout->descriptorSetLayout(),
      layout->bindingCount(),
      layout->bindings(),
      m_descriptors.data());
  }
  
  
  void DxvkContext::updateDynamicState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyDynamicState)) {
      m_flags.clr(DxvkContextFlag::GpDirtyDynamicState);
      
      this->updateViewports();
      this->updateBlendConstants();
      this->updateStencilReference();
    }
  }
  
  
  void DxvkContext::updateViewports() {
    m_cmd->cmdSetViewport(0, m_state.vp.viewportCount, m_state.vp.viewports.data());
    m_cmd->cmdSetScissor (0, m_state.vp.viewportCount, m_state.vp.scissorRects.data());
  }
  
  
  void DxvkContext::updateBlendConstants() {
    m_cmd->cmdSetBlendConstants(m_state.om.blendConstants);
  }
  
  
  void DxvkContext::updateStencilReference() {
    m_cmd->cmdSetStencilReference(
      VK_STENCIL_FRONT_AND_BACK,
      m_state.om.stencilReference);
  }
  
  
  void DxvkContext::updateIndexBufferBinding() {
    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer)) {
      m_flags.clr(DxvkContextFlag::GpDirtyIndexBuffer);
      
      if (m_state.vi.indexBuffer.handle() != VK_NULL_HANDLE) {
        m_cmd->cmdBindIndexBuffer(
          m_state.vi.indexBuffer.handle(),
          m_state.vi.indexBuffer.offset(),
          m_state.vi.indexType);
        m_cmd->trackResource(
          m_state.vi.indexBuffer.resource());
      }
    }
  }
  
  
  void DxvkContext::updateVertexBufferBindings() {
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers)) {
      m_flags.clr(DxvkContextFlag::GpDirtyVertexBuffers);
      
      for (uint32_t i = 0; i < m_state.il.numBindings; i++) {
        const DxvkBufferSlice& vbo = m_state.vi.vertexBuffers[i];
        
        const VkBuffer     handle = vbo.handle();
        const VkDeviceSize offset = vbo.offset();
        
        if (handle != VK_NULL_HANDLE) {
          m_cmd->cmdBindVertexBuffers(
            m_state.il.bindings[i].binding,
            1, &handle, &offset);
          m_cmd->trackResource(vbo.resource());
        } else {
          Logger::err(str::format("DxvkContext: Unbound vertex buffer: ", i));
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
    // TODO optimize. Each pipeline layout should
    // hold a list of resource that can be written.
    // TODO generalize so that this can be used for
    // graphics pipelines as well
    auto layout = m_state.cp.pipeline->layout();
    
    for (uint32_t i = 0; i < layout->bindingCount(); i++) {
      const DxvkDescriptorSlot binding = layout->binding(i);
      const DxvkShaderResourceSlot& slot = m_rc[binding.slot];
      
      if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        m_barriers.accessBuffer(
          slot.bufferSlice.buffer(),
          slot.bufferSlice.offset(),
          slot.bufferSlice.length(),
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_ACCESS_SHADER_READ_BIT | 
          VK_ACCESS_SHADER_WRITE_BIT,
          slot.bufferSlice.buffer()->info().stages,
          slot.bufferSlice.buffer()->info().access);
      } else if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
        m_barriers.accessBuffer(
          slot.bufferView->buffer(),
          slot.bufferView->info().rangeOffset,
          slot.bufferView->info().rangeLength,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_ACCESS_SHADER_READ_BIT | 
          VK_ACCESS_SHADER_WRITE_BIT,
          slot.bufferView->buffer()->info().stages,
          slot.bufferView->buffer()->info().access);
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
    
    m_barriers.recordCommands(m_cmd);
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
  
}