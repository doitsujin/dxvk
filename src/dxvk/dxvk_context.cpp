#include <cstring>
#include <vector>
#include <utility>

#include "dxvk_device.h"
#include "dxvk_context.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<DxvkDevice>& device, DxvkContextType type)
  : m_device      (device),
    m_type        (type),
    m_common      (&device->m_objects),
    m_sdmaAcquires(DxvkCmdBuffer::SdmaBuffer),
    m_sdmaBarriers(DxvkCmdBuffer::SdmaBuffer),
    m_initAcquires(DxvkCmdBuffer::InitBuffer),
    m_initBarriers(DxvkCmdBuffer::InitBuffer),
    m_execAcquires(DxvkCmdBuffer::ExecBuffer),
    m_execBarriers(DxvkCmdBuffer::ExecBuffer),
    m_queryManager(m_common->queryPool()) {
    // Init framebuffer info with default render pass in case
    // the app does not explicitly bind any render targets
    m_state.om.framebufferInfo = makeFramebufferInfo(m_state.om.renderTargets);
    m_descriptorManager = new DxvkDescriptorManager(device.ptr(), type);

    // Default destination barriers for graphics pipelines
    m_globalRoGraphicsBarrier.stages = m_device->getShaderPipelineStages()
                                     | VK_PIPELINE_STAGE_TRANSFER_BIT
                                     | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                     | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                     | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    m_globalRoGraphicsBarrier.access = 0;

    if (m_device->features().extTransformFeedback.transformFeedback)
      m_globalRoGraphicsBarrier.stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

    m_globalRwGraphicsBarrier = m_globalRoGraphicsBarrier;
    m_globalRwGraphicsBarrier.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                                     |  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    m_globalRwGraphicsBarrier.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT
                                     |  VK_ACCESS_INDEX_READ_BIT
                                     |  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                                     |  VK_ACCESS_UNIFORM_READ_BIT
                                     |  VK_ACCESS_SHADER_READ_BIT
                                     |  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                     |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                     |  VK_ACCESS_TRANSFER_READ_BIT;

    if (m_device->features().extTransformFeedback.transformFeedback)
      m_globalRwGraphicsBarrier.access |= VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT;

    // Store the lifetime tracking bit as a context feature so
    // that we don't have to scan device features at draw time
    if (m_device->mustTrackPipelineLifetime())
      m_features.set(DxvkContextFeature::TrackGraphicsPipeline);

    // Variable multisample rate is needed to efficiently support
    // rendering without bound render targets, otherwise we may
    // have to interrupt the current render pass whenever the
    // requested rasterizer sample count changes
    if (m_device->features().core.features.variableMultisampleRate)
      m_features.set(DxvkContextFeature::VariableMultisampleRate);

    // Maintenance5 introduced a bounded BindIndexBuffer function
    if (m_device->features().khrMaintenance5.maintenance5)
      m_features.set(DxvkContextFeature::IndexBufferRobustness);
  }
  
  
  DxvkContext::~DxvkContext() {
    
  }
  
  
  void DxvkContext::beginRecording(const Rc<DxvkCommandList>& cmdList) {
    m_cmd = cmdList;
    m_cmd->init();

    if (m_descriptorPool == nullptr)
      m_descriptorPool = m_descriptorManager->getDescriptorPool();

    this->beginCurrentCommands();
  }
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording() {
    this->endCurrentCommands();

    if (m_descriptorPool->shouldSubmit(false)) {
      m_cmd->trackDescriptorPool(m_descriptorPool, m_descriptorManager);
      m_descriptorPool = m_descriptorManager->getDescriptorPool();
    }

    m_cmd->finalize();
    return std::exchange(m_cmd, nullptr);
  }


  void DxvkContext::endFrame() {
    if (m_descriptorPool->shouldSubmit(true)) {
      m_cmd->trackDescriptorPool(m_descriptorPool, m_descriptorManager);
      m_descriptorPool = m_descriptorManager->getDescriptorPool();
    }
  }


  void DxvkContext::flushCommandList(DxvkSubmitStatus* status) {
    m_device->submitCommandList(
      this->endRecording(), status);
    
    this->beginRecording(
      m_device->createCommandList());
  }


  DxvkContextObjects DxvkContext::beginExternalRendering() {
    // Flush and invalidate everything
    endCurrentCommands();
    beginCurrentCommands();

    DxvkContextObjects result;
    result.cmd = m_cmd;
    result.descriptorPool = m_descriptorPool;
    return result;
  }

  
  void DxvkContext::beginQuery(const Rc<DxvkGpuQuery>& query) {
    m_queryManager.enableQuery(m_cmd, query);
  }


  void DxvkContext::endQuery(const Rc<DxvkGpuQuery>& query) {
    m_queryManager.disableQuery(m_cmd, query);
  }
  
  
  void DxvkContext::blitImageView(
    const Rc<DxvkImageView>&    dstView,
    const VkOffset3D*           dstOffsets,
    const Rc<DxvkImageView>&    srcView,
    const VkOffset3D*           srcOffsets,
          VkFilter              filter) {
    this->spillRenderPass(true);
    this->prepareImage(dstView->image(), dstView->imageSubresources());
    this->prepareImage(srcView->image(), srcView->imageSubresources());

    auto mapping = util::resolveSrcComponentMapping(
      dstView->info().unpackSwizzle(),
      srcView->info().unpackSwizzle());

    bool useFb = dstView->image()->info().sampleCount != VK_SAMPLE_COUNT_1_BIT
              || dstView->image()->info().format != dstView->info().format
              || srcView->image()->info().format != srcView->info().format
              || !util::isIdentityMapping(mapping);

    // Use render pass path if we already have the correct usage flags anyway
    useFb |= (dstView->info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
          && (srcView->info().usage & (VK_IMAGE_USAGE_SAMPLED_BIT));

    if (!useFb) {
      // Otherwise, verify that the vkCmdBlit path is supported for the given formats
      auto dstFeatures = m_device->adapter()->getFormatFeatures(dstView->info().format);
      auto srcFeatures = m_device->adapter()->getFormatFeatures(srcView->info().format);

      auto dstBits = dstView->image()->info().tiling == VK_IMAGE_TILING_OPTIMAL ? dstFeatures.optimal : dstFeatures.linear;
      auto srcBits = srcView->image()->info().tiling == VK_IMAGE_TILING_OPTIMAL ? srcFeatures.optimal : srcFeatures.linear;

      useFb |= !(dstBits & VK_FORMAT_FEATURE_2_BLIT_DST_BIT)
            || !(srcBits & VK_FORMAT_FEATURE_2_BLIT_SRC_BIT);
    }

    if (useFb) {
      this->blitImageFb(dstView, dstOffsets,
        srcView, srcOffsets, filter);
    } else {
      this->blitImageHw(dstView, dstOffsets,
        srcView, srcOffsets, filter);
    }
  }


  void DxvkContext::changeImageLayout(
    const Rc<DxvkImage>&        image,
          VkImageLayout         layout) {
    if (image->info().layout != layout) {
      this->spillRenderPass(true);

      VkImageSubresourceRange subresources = image->getAvailableSubresources();

      this->prepareImage(image, subresources);

      if (m_execBarriers.isImageDirty(image, subresources, DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);

      m_execBarriers.accessImage(image, subresources,
        image->info().layout,
        image->info().stages, 0,
        layout,
        image->info().stages,
        image->info().access);

      image->setLayout(layout);

      for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
        const DxvkAttachment& rt = m_state.om.renderTargets.color[i];
        if (rt.view != nullptr && rt.view->image() == image) {
          m_rtLayouts.color[i] = layout;
        }
      }

      const DxvkAttachment& ds = m_state.om.renderTargets.depth;
      if (ds.view != nullptr && ds.view->image() == image) {
        m_rtLayouts.depth = layout;
      }

      m_cmd->trackResource<DxvkAccess::Write>(image);
    }
  }


  void DxvkContext::clearBuffer(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          uint32_t              value) {
    bool replaceBuffer = this->tryInvalidateDeviceLocalBuffer(buffer, length);
    auto bufferSlice = buffer->getSliceHandle(offset, align(length, sizeof(uint32_t)));

    if (!replaceBuffer) {
      this->spillRenderPass(true);
    
      if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);
    }

    DxvkCmdBuffer cmdBuffer = replaceBuffer
      ? DxvkCmdBuffer::InitBuffer
      : DxvkCmdBuffer::ExecBuffer;

    if (length > sizeof(value)) {
      m_cmd->cmdFillBuffer(cmdBuffer,
        bufferSlice.handle,
        bufferSlice.offset,
        bufferSlice.length,
        value);
    } else {
      m_cmd->cmdUpdateBuffer(cmdBuffer,
        bufferSlice.handle,
        bufferSlice.offset,
        bufferSlice.length,
        &value);
    }

    auto& barriers = replaceBuffer
      ? m_initBarriers
      : m_execBarriers;

    barriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(buffer);
  }
  
  
  void DxvkContext::clearBufferView(
    const Rc<DxvkBufferView>&   bufferView,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          VkClearColorValue     value) {
    this->spillRenderPass(true);
    this->invalidateState();

    auto bufferSlice = bufferView->getSliceHandle();

    if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);
    
    // Query pipeline objects to use for this clear operation
    DxvkMetaClearPipeline pipeInfo = m_common->metaClear().getClearBufferPipeline(
      lookupFormatInfo(bufferView->info().format)->flags);
    
    // Create a descriptor set pointing to the view
    VkBufferView viewObject = bufferView->handle();
    
    VkDescriptorSet descriptorSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);
    
    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    descriptorWrite.pTexelBufferView = &viewObject;
    m_cmd->updateDescriptorSets(1, &descriptorWrite);
    
    // Prepare shader arguments
    DxvkMetaClearArgs pushArgs = { };
    pushArgs.clearValue = value;
    pushArgs.offset = VkOffset3D {  int32_t(offset), 0, 0 };
    pushArgs.extent = VkExtent3D { uint32_t(length), 1, 1 };
    
    VkExtent3D workgroups = util::computeBlockCount(
      pushArgs.extent, pipeInfo.workgroupSize);
    
    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      workgroups.width, workgroups.height, workgroups.depth);
    
    m_execBarriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      bufferView->buffer()->info().stages,
      bufferView->buffer()->info().access);
    
    m_cmd->trackResource<DxvkAccess::Write>(bufferView->buffer());
  }
  
  
  void DxvkContext::clearRenderTarget(
    const Rc<DxvkImageView>&    imageView,
          VkImageAspectFlags    clearAspects,
          VkClearValue          clearValue) {
    // Make sure the color components are ordered correctly
    if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
      clearValue.color = util::swizzleClearColor(clearValue.color,
        util::invertComponentMapping(imageView->info().unpackSwizzle()));
    }
    
    // Check whether the render target view is an attachment
    // of the current framebuffer and is included entirely.
    // If not, we need to create a temporary framebuffer.
    int32_t attachmentIndex = -1;
    
    if (m_state.om.framebufferInfo.isFullSize(imageView))
      attachmentIndex = m_state.om.framebufferInfo.findAttachment(imageView);

    if (attachmentIndex < 0) {
      // Suspend works here because we'll end up with one of these scenarios:
      // 1) The render pass gets ended for good, in which case we emit barriers
      // 2) The clear gets folded into render pass ops, so the layout is correct
      // 3) The clear gets executed separately, in which case updateFramebuffer
      //    will indirectly emit barriers for the given render target.
      // If there is overlap, we need to explicitly transition affected attachments.
      this->spillRenderPass(true);
      this->prepareImage(imageView->image(), imageView->subresources(), false);
    } else if (!m_state.om.framebufferInfo.isWritable(attachmentIndex, clearAspects)) {
      // We cannot inline clears if the clear aspects are not writable
      this->spillRenderPass(true);
    }

    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      uint32_t colorIndex = std::max(0, m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex));

      VkClearAttachment clearInfo;
      clearInfo.aspectMask      = clearAspects;
      clearInfo.colorAttachment = colorIndex;
      clearInfo.clearValue      = clearValue;

      VkClearRect clearRect;
      clearRect.rect.offset.x       = 0;
      clearRect.rect.offset.y       = 0;
      clearRect.rect.extent.width   = imageView->mipLevelExtent(0).width;
      clearRect.rect.extent.height  = imageView->mipLevelExtent(0).height;
      clearRect.baseArrayLayer      = 0;
      clearRect.layerCount          = imageView->info().layerCount;

      m_cmd->cmdClearAttachments(1, &clearInfo, 1, &clearRect);
    } else
      this->deferClear(imageView, clearAspects, clearValue);
  }
  
  
  void DxvkContext::clearImageView(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkImageAspectFlags    aspect,
          VkClearValue          value) {
    const VkImageUsageFlags viewUsage = imageView->info().usage;

    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
      value.color = util::swizzleClearColor(value.color,
        util::invertComponentMapping(imageView->info().unpackSwizzle()));
    }
    
    if (viewUsage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      this->clearImageViewFb(imageView, offset, extent, aspect, value);
    else if (viewUsage & VK_IMAGE_USAGE_STORAGE_BIT)
      this->clearImageViewCs(imageView, offset, extent, value);
  }
  
  
  void DxvkContext::copyBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkDeviceSize          numBytes) {
    // When overwriting small buffers, we can allocate a new slice in order to
    // avoid suspending the current render pass or inserting barriers. The source
    // buffer must be read-only since otherwise we cannot schedule the copy early.
    bool srcIsReadOnly = DxvkBarrierSet::getAccessTypes(srcBuffer->info().access) == DxvkAccess::Read;
    bool replaceBuffer = srcIsReadOnly && this->tryInvalidateDeviceLocalBuffer(dstBuffer, numBytes);

    auto srcSlice = srcBuffer->getSliceHandle(srcOffset, numBytes);
    auto dstSlice = dstBuffer->getSliceHandle(dstOffset, numBytes);

    if (!replaceBuffer) {
      this->spillRenderPass(true);

      if (m_execBarriers.isBufferDirty(srcSlice, DxvkAccess::Read)
       || m_execBarriers.isBufferDirty(dstSlice, DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);
    }

    DxvkCmdBuffer cmdBuffer = replaceBuffer
      ? DxvkCmdBuffer::InitBuffer
      : DxvkCmdBuffer::ExecBuffer;

    VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copyRegion.srcOffset = srcSlice.offset;
    copyRegion.dstOffset = dstSlice.offset;
    copyRegion.size      = dstSlice.length;

    VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo.srcBuffer = srcSlice.handle;
    copyInfo.dstBuffer = dstSlice.handle;
    copyInfo.regionCount = 1;
    copyInfo.pRegions = &copyRegion;

    m_cmd->cmdCopyBuffer(cmdBuffer, &copyInfo);

    auto& barriers = replaceBuffer
      ? m_initBarriers
      : m_execBarriers;

    barriers.accessBuffer(srcSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcBuffer->info().stages,
      srcBuffer->info().access);

    barriers.accessBuffer(dstSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstBuffer->info().stages,
      dstBuffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstBuffer);
    m_cmd->trackResource<DxvkAccess::Read>(srcBuffer);
  }
  
  
  void DxvkContext::copyBufferRegion(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
          VkDeviceSize          srcOffset,
          VkDeviceSize          numBytes) {
    VkDeviceSize loOvl = std::max(dstOffset, srcOffset);
    VkDeviceSize hiOvl = std::min(dstOffset, srcOffset) + numBytes;

    if (hiOvl > loOvl) {
      DxvkBufferCreateInfo bufInfo;
      bufInfo.size    = numBytes;
      bufInfo.usage   = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                      | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bufInfo.stages  = VK_PIPELINE_STAGE_TRANSFER_BIT;
      bufInfo.access  = VK_ACCESS_TRANSFER_WRITE_BIT
                      | VK_ACCESS_TRANSFER_READ_BIT;

      auto tmpBuffer = m_device->createBuffer(
        bufInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      
      VkDeviceSize tmpOffset = 0;
      
      this->copyBuffer(tmpBuffer, tmpOffset, dstBuffer, srcOffset, numBytes);
      this->copyBuffer(dstBuffer, dstOffset, tmpBuffer, tmpOffset, numBytes);
    } else {
      this->copyBuffer(dstBuffer, dstOffset, dstBuffer, srcOffset, numBytes);
    }
  }


  void DxvkContext::copyBufferToImage(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
          VkExtent3D            dstExtent,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkDeviceSize          rowAlignment,
          VkDeviceSize          sliceAlignment,
          VkFormat              srcFormat) {
    bool useFb = !formatsAreCopyCompatible(dstImage->info().format, srcFormat);

    if (useFb) {
      copyBufferToImageFb(dstImage, dstSubresource, dstOffset, dstExtent,
        srcBuffer, srcOffset, rowAlignment, sliceAlignment,
        srcFormat ? srcFormat : dstImage->info().format);
    } else {
      copyBufferToImageHw(dstImage, dstSubresource, dstOffset, dstExtent,
        srcBuffer, srcOffset, rowAlignment, sliceAlignment);
    }
  }
  
  
  void DxvkContext::copyImage(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    this->spillRenderPass(true);

    if (this->copyImageClear(dstImage, dstSubresource, dstOffset, extent, srcImage, srcSubresource))
      return;

    this->prepareImage(dstImage, vk::makeSubresourceRange(dstSubresource));
    this->prepareImage(srcImage, vk::makeSubresourceRange(srcSubresource));

    bool useFb = dstSubresource.aspectMask != srcSubresource.aspectMask;

    if (m_device->perfHints().preferFbDepthStencilCopy) {
      useFb |= (dstSubresource.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            && (dstImage->info().usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            && (srcImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    if (!useFb) {
      this->copyImageHw(
        dstImage, dstSubresource, dstOffset,
        srcImage, srcSubresource, srcOffset,
        extent);
    } else {
      this->copyImageFb(
        dstImage, dstSubresource, dstOffset,
        srcImage, srcSubresource, srcOffset,
        extent);
    }
  }
  
  
  void DxvkContext::copyImageRegion(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    VkOffset3D loOvl = {
      std::max(dstOffset.x, srcOffset.x),
      std::max(dstOffset.y, srcOffset.y),
      std::max(dstOffset.z, srcOffset.z) };
    
    VkOffset3D hiOvl = {
      std::min(dstOffset.x, srcOffset.x) + int32_t(extent.width),
      std::min(dstOffset.y, srcOffset.y) + int32_t(extent.height),
      std::min(dstOffset.z, srcOffset.z) + int32_t(extent.depth) };
    
    bool overlap = hiOvl.x > loOvl.x
                && hiOvl.y > loOvl.y
                && hiOvl.z > loOvl.z;
    
    if (overlap) {
      DxvkImageCreateInfo imgInfo;
      imgInfo.type          = dstImage->info().type;
      imgInfo.format        = dstImage->info().format;
      imgInfo.flags         = 0;
      imgInfo.sampleCount   = dstImage->info().sampleCount;
      imgInfo.extent        = extent;
      imgInfo.numLayers     = dstSubresource.layerCount;
      imgInfo.mipLevels     = 1;
      imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      imgInfo.stages        = VK_PIPELINE_STAGE_TRANSFER_BIT;
      imgInfo.access        = VK_ACCESS_TRANSFER_WRITE_BIT
                            | VK_ACCESS_TRANSFER_READ_BIT;
      imgInfo.tiling        = dstImage->info().tiling;
      imgInfo.layout        = VK_IMAGE_LAYOUT_GENERAL;

      auto tmpImage = m_device->createImage(
        imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      
      VkImageSubresourceLayers tmpSubresource;
      tmpSubresource.aspectMask     = dstSubresource.aspectMask;
      tmpSubresource.mipLevel       = 0;
      tmpSubresource.baseArrayLayer = 0;
      tmpSubresource.layerCount     = dstSubresource.layerCount;

      VkOffset3D tmpOffset = { 0, 0, 0 };

      this->copyImage(
        tmpImage, tmpSubresource, tmpOffset,
        dstImage, dstSubresource, srcOffset,
        extent);
      
      this->copyImage(
        dstImage, dstSubresource, dstOffset,
        tmpImage, tmpSubresource, tmpOffset,
        extent);
    } else {
      this->copyImage(
        dstImage, dstSubresource, dstOffset,
        dstImage, dstSubresource, srcOffset,
        extent);
    }
  }


  void DxvkContext::copyImageToBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
          VkDeviceSize          rowAlignment,
          VkDeviceSize          sliceAlignment,
          VkFormat              dstFormat,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            srcExtent) {
    bool useFb = !formatsAreCopyCompatible(srcImage->info().format, dstFormat);

    if (useFb) {
      copyImageToBufferFb(dstBuffer, dstOffset, rowAlignment, sliceAlignment,
        dstFormat ? dstFormat : srcImage->info().format,
        srcImage, srcSubresource, srcOffset, srcExtent);
    } else {
      copyImageToBufferHw(dstBuffer, dstOffset, rowAlignment, sliceAlignment,
        srcImage, srcSubresource, srcOffset, srcExtent);
    }
  }


  void DxvkContext::copyPackedBufferImage(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstBufferOffset,
          VkOffset3D            dstOffset,
          VkExtent3D            dstSize,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcBufferOffset,
          VkOffset3D            srcOffset,
          VkExtent3D            srcSize,
          VkExtent3D            extent,
          VkDeviceSize          elementSize) {
    this->spillRenderPass(true);
    this->invalidateState();

    auto dstBufferSlice = dstBuffer->getSliceHandle(dstBufferOffset, elementSize * util::flattenImageExtent(dstSize));
    auto srcBufferSlice = srcBuffer->getSliceHandle(srcBufferOffset, elementSize * util::flattenImageExtent(srcSize));

    if (m_execBarriers.isBufferDirty(dstBufferSlice, DxvkAccess::Write)
     || m_execBarriers.isBufferDirty(srcBufferSlice, DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);

    // We'll use texel buffer views with an appropriately
    // sized integer format to perform the copy
    VkFormat format = VK_FORMAT_UNDEFINED;

    switch (elementSize) {
      case  1: format = VK_FORMAT_R8_UINT; break;
      case  2: format = VK_FORMAT_R16_UINT; break;
      case  4: format = VK_FORMAT_R32_UINT; break;
      case  8: format = VK_FORMAT_R32G32_UINT; break;
      case 12: format = VK_FORMAT_R32G32B32_UINT; break;
      case 16: format = VK_FORMAT_R32G32B32A32_UINT; break;
    }

    if (!format) {
      Logger::err(str::format("DxvkContext: copyPackedBufferImage: Unsupported element size ", elementSize));
      return;
    }

    DxvkBufferViewKey viewInfo;
    viewInfo.format = format;
    viewInfo.offset = dstBufferOffset;
    viewInfo.size = dstBufferSlice.length;
    viewInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    Rc<DxvkBufferView> dstView = dstBuffer->createView(viewInfo);

    viewInfo.offset = srcBufferOffset;
    viewInfo.size = srcBufferSlice.length;
    viewInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    Rc<DxvkBufferView> srcView;

    if (srcBuffer == dstBuffer
     && srcBufferSlice.offset < dstBufferSlice.offset + dstBufferSlice.length
     && srcBufferSlice.offset + srcBufferSlice.length > dstBufferSlice.offset) {
      // Create temporary copy in case of overlapping regions
      DxvkBufferCreateInfo bufferInfo;
      bufferInfo.size   = srcBufferSlice.length;
      bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                        | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                        | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      bufferInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
                        | VK_ACCESS_SHADER_READ_BIT;
      Rc<DxvkBuffer> tmpBuffer = m_device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      auto tmpBufferSlice = tmpBuffer->getSliceHandle();

      VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
      copyRegion.srcOffset = srcBufferSlice.offset;
      copyRegion.dstOffset = tmpBufferSlice.offset;
      copyRegion.size = tmpBufferSlice.length;

      VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
      copyInfo.srcBuffer = srcBufferSlice.handle;
      copyInfo.dstBuffer = tmpBufferSlice.handle;
      copyInfo.regionCount = 1;
      copyInfo.pRegions = &copyRegion;

      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &copyInfo);

      emitMemoryBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);

      viewInfo.offset = 0;
      srcView = tmpBuffer->createView(viewInfo);

      m_cmd->trackResource<DxvkAccess::Write>(tmpBuffer);
    } else {
      srcView = srcBuffer->createView(viewInfo);
    }

    auto pipeInfo = m_common->metaCopy().getCopyFormattedBufferPipeline();
    VkDescriptorSet descriptorSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites;

    std::array<std::pair<VkDescriptorType, VkBufferView>, 2> descriptorInfos = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, dstView->handle() },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, srcView->handle() },
    }};

    for (uint32_t i = 0; i < descriptorWrites.size(); i++) {
      auto write = &descriptorWrites[i];
      auto info = &descriptorInfos[i];

      write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write->pNext = nullptr;
      write->dstSet = descriptorSet;
      write->dstBinding = i;
      write->dstArrayElement = 0;
      write->descriptorCount = 1;
      write->descriptorType = info->first;
      write->pImageInfo = nullptr;
      write->pBufferInfo = nullptr;
      write->pTexelBufferView = &info->second;
    }

    m_cmd->updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data());

    DxvkFormattedBufferCopyArgs args = { };
    args.dstOffset = dstOffset;
    args.srcOffset = srcOffset;
    args.extent = extent;
    args.dstSize = { dstSize.width, dstSize.height };
    args.srcSize = { srcSize.width, srcSize.height };

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeHandle);
    
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    
    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(args), &args);
    
    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      (extent.width  + 7) / 8,
      (extent.height + 7) / 8,
      extent.depth);
    
    m_execBarriers.accessBuffer(
      dstView->getSliceHandle(),
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      dstBuffer->info().stages,
      dstBuffer->info().access);

    m_execBarriers.accessBuffer(
      srcView->getSliceHandle(),
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      srcBuffer->info().stages,
      srcBuffer->info().access);

    // Track all involved resources
    m_cmd->trackResource<DxvkAccess::Write>(dstBuffer);
    m_cmd->trackResource<DxvkAccess::Read>(srcBuffer);
  }


  void DxvkContext::copySparsePagesToBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
    const Rc<DxvkPagedResource>& srcResource,
          uint32_t              pageCount,
    const uint32_t*             pages) {
    this->copySparsePages<true>(
      srcResource, pageCount, pages,
      dstBuffer, dstOffset);
  }


  void DxvkContext::copySparsePagesFromBuffer(
    const Rc<DxvkPagedResource>& dstResource,
          uint32_t              pageCount,
    const uint32_t*             pages,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset) {
    this->copySparsePages<false>(
      dstResource, pageCount, pages,
      srcBuffer, srcOffset);
  }


  void DxvkContext::discardImageView(
    const Rc<DxvkImageView>&      imageView,
          VkImageAspectFlags      discardAspects) {
    VkImageUsageFlags viewUsage = imageView->info().usage;

    // Ignore non-render target views since there's likely no good use case for
    // discarding those. Also, force reinitialization even if the image is bound
    // as a render target, which may have niche use cases for depth buffers.
    if (viewUsage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      this->spillRenderPass(true);
      this->deferDiscard(imageView, discardAspects);
    }
  }


  void DxvkContext::dispatch(
          uint32_t x,
          uint32_t y,
          uint32_t z) {
    if (this->commitComputeState()) {
      this->commitComputeBarriers<false>();
      this->commitComputeBarriers<true>();

      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
      
      m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer, x, y, z);
      
      m_queryManager.endQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1);
  }
  
  
  void DxvkContext::dispatchIndirect(
          VkDeviceSize      offset) {
    auto bufferSlice = m_state.id.argBuffer.getSliceHandle(
      offset, sizeof(VkDispatchIndirectCommand));

    if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);
    
    if (this->commitComputeState()) {
      this->commitComputeBarriers<false>();
      this->commitComputeBarriers<true>();

      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
      
      m_cmd->cmdDispatchIndirect(DxvkCmdBuffer::ExecBuffer,
        bufferSlice.handle, bufferSlice.offset);
      
      m_queryManager.endQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
      
      m_execBarriers.accessBuffer(bufferSlice,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        m_state.id.argBuffer.bufferInfo().stages,
        m_state.id.argBuffer.bufferInfo().access);
      
      this->trackDrawBuffer();
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1);
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    if (this->commitGraphicsState<false, false>()) {
      m_cmd->cmdDraw(
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndirect(
          VkDeviceSize      offset,
          uint32_t          count,
          uint32_t          stride) {
    if (this->commitGraphicsState<false, true>()) {
      auto descriptor = m_state.id.argBuffer.getDescriptor();
      
      m_cmd->cmdDrawIndirect(
        descriptor.buffer.buffer,
        descriptor.buffer.offset + offset,
        count, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndirectCount(
          VkDeviceSize      offset,
          VkDeviceSize      countOffset,
          uint32_t          maxCount,
          uint32_t          stride) {
    if (this->commitGraphicsState<false, true>()) {
      auto argDescriptor = m_state.id.argBuffer.getDescriptor();
      auto cntDescriptor = m_state.id.cntBuffer.getDescriptor();
      
      m_cmd->cmdDrawIndirectCount(
        argDescriptor.buffer.buffer,
        argDescriptor.buffer.offset + offset,
        cntDescriptor.buffer.buffer,
        cntDescriptor.buffer.offset + countOffset,
        maxCount, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          int32_t  vertexOffset,
          uint32_t firstInstance) {
    if (this->commitGraphicsState<true, false>()) {
      m_cmd->cmdDrawIndexed(
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndexedIndirect(
          VkDeviceSize      offset,
          uint32_t          count,
          uint32_t          stride) {
    if (this->commitGraphicsState<true, true>()) {
      auto descriptor = m_state.id.argBuffer.getDescriptor();
      
      m_cmd->cmdDrawIndexedIndirect(
        descriptor.buffer.buffer,
        descriptor.buffer.offset + offset,
        count, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndexedIndirectCount(
          VkDeviceSize      offset,
          VkDeviceSize      countOffset,
          uint32_t          maxCount,
          uint32_t          stride) {
    if (this->commitGraphicsState<true, true>()) {
      auto argDescriptor = m_state.id.argBuffer.getDescriptor();
      auto cntDescriptor = m_state.id.cntBuffer.getDescriptor();
      
      m_cmd->cmdDrawIndexedIndirectCount(
        argDescriptor.buffer.buffer,
        argDescriptor.buffer.offset + offset,
        cntDescriptor.buffer.buffer,
        cntDescriptor.buffer.offset + countOffset,
        maxCount, stride);
    }
    
    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }
  
  
  void DxvkContext::drawIndirectXfb(
    const DxvkBufferSlice&  counterBuffer,
          uint32_t          counterDivisor,
          uint32_t          counterBias) {
    if (this->commitGraphicsState<false, false>()) {
      auto physSlice = counterBuffer.getSliceHandle();

      m_cmd->cmdDrawIndirectVertexCount(1, 0,
        physSlice.handle,
        physSlice.offset,
        counterBias,
        counterDivisor);
    }

    m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1);
  }


  void DxvkContext::initBuffer(
    const Rc<DxvkBuffer>&           buffer) {
    auto slice = buffer->getSliceHandle();

    m_cmd->cmdFillBuffer(DxvkCmdBuffer::InitBuffer,
      slice.handle, slice.offset,
      dxvk::align(slice.length, 4), 0);

    m_initBarriers.accessMemory(
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages, buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(buffer);
  }


  void DxvkContext::initImage(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             initialLayout) {
    if (initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
      m_initBarriers.accessImage(image, subresources,
        initialLayout,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
        image->info().layout,
        image->info().stages,
        image->info().access);

      m_cmd->trackResource<DxvkAccess::None>(image);
    } else {
      VkImageLayout clearLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      m_initAcquires.accessImage(image, subresources,
        initialLayout,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
        clearLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);

      m_initAcquires.recordCommands(m_cmd);

      auto formatInfo = image->formatInfo();

      if (formatInfo->flags.any(DxvkFormatFlag::BlockCompressed, DxvkFormatFlag::MultiPlane)) {
        for (auto aspects = formatInfo->aspectMask; aspects; ) {
          auto aspect = vk::getNextAspect(aspects);
          auto extent = image->mipLevelExtent(subresources.baseMipLevel);
          auto elementSize = formatInfo->elementSize;

          if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
            auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
            extent.width  /= plane->blockSize.width;
            extent.height /= plane->blockSize.height;
            elementSize = plane->elementSize;
          }

          // Allocate enough staging buffer memory to fit one
          // single subresource, then dispatch multiple copies
          VkExtent3D blockCount = util::computeBlockCount(extent, formatInfo->blockSize);
          VkDeviceSize dataSize = util::flattenImageExtent(blockCount) * elementSize;

          auto zeroBuffer = createZeroBuffer(dataSize);
          auto zeroHandle = zeroBuffer->getSliceHandle();

          for (uint32_t level = 0; level < subresources.levelCount; level++) {
            VkOffset3D offset = VkOffset3D { 0, 0, 0 };
            VkExtent3D extent = image->mipLevelExtent(subresources.baseMipLevel + level);

            if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
              auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
              extent.width  /= plane->blockSize.width;
              extent.height /= plane->blockSize.height;
            }

            for (uint32_t layer = 0; layer < subresources.layerCount; layer++) {
              VkBufferImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
              copyRegion.bufferOffset = zeroHandle.offset;
              copyRegion.imageSubresource = vk::makeSubresourceLayers(
                vk::pickSubresource(subresources, level, layer));
              copyRegion.imageSubresource.aspectMask = aspect;
              copyRegion.imageOffset = offset;
              copyRegion.imageExtent = extent;

              VkCopyBufferToImageInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
              copyInfo.srcBuffer = zeroHandle.handle;
              copyInfo.dstImage = image->handle();
              copyInfo.dstImageLayout = clearLayout;
              copyInfo.regionCount = 1;
              copyInfo.pRegions = &copyRegion;

              m_cmd->cmdCopyBufferToImage(DxvkCmdBuffer::InitBuffer, &copyInfo);
            }
          }

          m_cmd->trackResource<DxvkAccess::Read>(zeroBuffer);
        }
      } else {
        if (subresources.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
          VkClearDepthStencilValue value = { };

          m_cmd->cmdClearDepthStencilImage(DxvkCmdBuffer::InitBuffer,
            image->handle(), clearLayout, &value, 1, &subresources);
        } else {
          VkClearColorValue value = { };

          m_cmd->cmdClearColorImage(DxvkCmdBuffer::InitBuffer,
            image->handle(), clearLayout, &value, 1, &subresources);
        }
      }

      m_initBarriers.accessImage(image, subresources,
        clearLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        image->info().layout,
        image->info().stages,
        image->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(image);
    }
  }
  
  
  void DxvkContext::initSparseImage(
    const Rc<DxvkImage>&            image) {
    auto vk = m_device->vkd();

    // Query sparse memory requirements
    uint32_t reqCount = 0;
    vk->vkGetImageSparseMemoryRequirements(vk->device(), image->handle(), &reqCount, nullptr);

    std::vector<VkSparseImageMemoryRequirements> req(reqCount);
    vk->vkGetImageSparseMemoryRequirements(vk->device(), image->handle(), &reqCount, req.data());

    // Bind metadata aspects. Since the image was just created,
    // we do not need to interrupt our command list for that.
    DxvkResourceMemoryInfo memoryInfo = image->getMemoryInfo();

    for (const auto& r : req) {
      if (!(r.formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT))
        continue;

      uint32_t layerCount = (r.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT)
        ? 1u : image->info().numLayers;

      for (uint32_t i = 0; i < layerCount; i++) {
        DxvkSparseImageOpaqueBindKey key;
        key.image   = image->handle();
        key.offset  = r.imageMipTailOffset + i * r.imageMipTailStride;
        key.size    = r.imageMipTailSize;
        key.flags   = VK_SPARSE_MEMORY_BIND_METADATA_BIT;

        DxvkResourceMemoryInfo page;
        page.memory = memoryInfo.memory;
        page.offset = memoryInfo.offset;
        page.size = r.imageMipTailSize;

        m_cmd->bindImageOpaqueMemory(key, page);

        memoryInfo.offset += r.imageMipTailSize;
      }
    }

    // Perform initial layout transition
    m_initBarriers.accessImage(image,
      image->getAvailableSubresources(),
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
      image->info().layout,
      image->info().stages,
      image->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(image);
  }


  void DxvkContext::emitGraphicsBarrier(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    // Emit barrier early so we can fold this into
    // the spill render pass barrier if possible
    if (srcStages | dstStages) {
      m_execBarriers.accessMemory(
        srcStages, srcAccess,
        dstStages, dstAccess);
    }

    this->spillRenderPass(true);

    // Flush barriers if there was no active render pass.
    // This is necessary because there are no resources
    // associated with the barrier to allow tracking.
    if (srcStages | dstStages)
      m_execBarriers.recordCommands(m_cmd);
  }


  void DxvkContext::emitBufferBarrier(
    const Rc<DxvkBuffer>&           resource,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    this->spillRenderPass(true);

    m_execBarriers.accessBuffer(resource->getSliceHandle(),
      srcStages, srcAccess, dstStages, dstAccess);

    m_cmd->trackResource<DxvkAccess::Write>(resource);
  }


  void DxvkContext::emitImageBarrier(
    const Rc<DxvkImage>&            resource,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    this->spillRenderPass(true);
    this->prepareImage(resource, resource->getAvailableSubresources());

    if (m_execBarriers.isImageDirty(resource, resource->getAvailableSubresources(), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    m_execBarriers.accessImage(
      resource, resource->getAvailableSubresources(),
      srcLayout, srcStages, srcAccess,
      dstLayout, dstStages, dstAccess);

    m_cmd->trackResource<DxvkAccess::Write>(resource);
  }


  void DxvkContext::generateMipmaps(
    const Rc<DxvkImageView>&        imageView,
          VkFilter                  filter) {
    if (imageView->info().mipCount <= 1)
      return;
    
    this->spillRenderPass(false);
    this->invalidateState();

    // Make sure we can both render to and read from the image
    VkFormat viewFormat = imageView->info().format;

    DxvkImageUsageInfo usageInfo;
    usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    usageInfo.viewFormatCount = 1;
    usageInfo.viewFormats = &viewFormat;

    if (!ensureImageCompatibility(imageView->image(), usageInfo)) {
      Logger::err(str::format("DxvkContext: generateMipmaps: Unsupported operation:"
        "\n  view format:  ", imageView->info().format,
        "\n  image format: ", imageView->image()->info().format));
      return;
    }

    if (m_execBarriers.isImageDirty(imageView->image(), imageView->imageSubresources(), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Create image views, etc.
    DxvkMetaMipGenViews mipGenerator(imageView);
    
    VkImageLayout dstLayout = imageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkImageLayout srcLayout = imageView->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // If necessary, transition first mip level to the read-only layout
    if (imageView->image()->info().layout != srcLayout) {
      m_execAcquires.accessImage(imageView->image(),
        mipGenerator.getTopSubresource(),
        imageView->image()->info().layout,
        imageView->image()->info().stages, 0,
        srcLayout,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    }

    // If necessary, initialize all levels that are written to
    if (imageView->image()->info().layout != dstLayout) {
      m_execAcquires.accessImage(imageView->image(),
        mipGenerator.getAllTargetSubresources(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        imageView->image()->info().stages, 0,
        dstLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);
    
    // Common descriptor set properties that we use to
    // bind the source image view to the fragment shader
    Rc<DxvkSampler> sampler = createBlitSampler(filter);

    VkDescriptorImageInfo descriptorImage = { };
    descriptorImage.sampler     = sampler->handle();
    descriptorImage.imageLayout = srcLayout;
    
    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo       = &descriptorImage;
    
    // Common render pass info
    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;
    
    // Retrieve a compatible pipeline to use for rendering
    DxvkMetaBlitPipeline pipeInfo = m_common->metaBlit().getPipeline(
      mipGenerator.getSrcViewType(), imageView->info().format, VK_SAMPLE_COUNT_1_BIT);
    
    for (uint32_t i = 0; i < mipGenerator.getPassCount(); i++) {
      // Width, height and layer count for the current pass
      VkExtent3D passExtent = mipGenerator.computePassExtent(i);
      
      // Create descriptor set with the current source view
      descriptorImage.imageView = mipGenerator.getSrcViewHandle(i);
      descriptorWrite.dstSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);
      m_cmd->updateDescriptorSets(1, &descriptorWrite);
      
      // Set up viewport and scissor rect
      VkViewport viewport;
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = float(passExtent.width);
      viewport.height   = float(passExtent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      
      VkRect2D scissor;
      scissor.offset    = { 0, 0 };
      scissor.extent    = { passExtent.width, passExtent.height };
      
      // Set up rendering info
      attachmentInfo.imageView = mipGenerator.getDstViewHandle(i);
      renderingInfo.renderArea = scissor;
      renderingInfo.layerCount = passExtent.depth;
      
      // Set up push constants
      DxvkMetaBlitPushConstants pushConstants = { };
      pushConstants.srcCoord0  = { 0.0f, 0.0f, 0.0f };
      pushConstants.srcCoord1  = { 1.0f, 1.0f, 1.0f };
      pushConstants.layerCount = passExtent.depth;

      if (i) {
        m_execAcquires.accessImage(imageView->image(),
          mipGenerator.getSourceSubresource(i),
          dstLayout,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          srcLayout,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          VK_ACCESS_SHADER_READ_BIT);
        m_execAcquires.recordCommands(m_cmd);
      }

      m_cmd->cmdBeginRendering(&renderingInfo);
      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeHandle);
      m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeLayout,
        descriptorWrite.dstSet, 0, nullptr);
      
      m_cmd->cmdSetViewport(1, &viewport);
      m_cmd->cmdSetScissor(1, &scissor);
      
      m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
        pipeInfo.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(pushConstants), &pushConstants);
      
      m_cmd->cmdDraw(3, passExtent.depth, 0, 0);
      m_cmd->cmdEndRendering();
    }

    // Issue barriers to ensure we can safely access all mip
    // levels of the image in all ways the image can be used
    if (srcLayout == dstLayout) {
      m_execBarriers.accessImage(imageView->image(),
        imageView->imageSubresources(),
        srcLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_SHADER_READ_BIT,
        imageView->image()->info().layout,
        imageView->image()->info().stages,
        imageView->image()->info().access);
    } else {
      m_execBarriers.accessImage(imageView->image(),
        mipGenerator.getAllSourceSubresources(),
        srcLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_SHADER_READ_BIT,
        imageView->image()->info().layout,
        imageView->image()->info().stages,
        imageView->image()->info().access);

      m_execBarriers.accessImage(imageView->image(),
        mipGenerator.getBottomSubresource(),
        dstLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        imageView->image()->info().layout,
        imageView->image()->info().stages,
        imageView->image()->info().access);
    }

    m_cmd->trackResource<DxvkAccess::Write>(imageView->image());
    m_cmd->trackSampler(sampler);
  }
  
  
  void DxvkContext::invalidateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          Rc<DxvkResourceAllocation>&& slice) {
    Rc<DxvkResourceAllocation> prevAllocation = buffer->assignSlice(std::move(slice));
    m_cmd->trackResource<DxvkAccess::None>(std::move(prevAllocation));

    // We also need to update all bindings that the buffer
    // may be bound to either directly or through views.
    VkBufferUsageFlags usage = buffer->info().usage &
      ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    // Fast early-out for plain uniform buffers, very common
    if (likely(usage == VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
      m_descriptorState.dirtyBuffers(buffer->getShaderStages());
      return;
    }

    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
      m_descriptorState.dirtyBuffers(buffer->getShaderStages());

    if (usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
      m_descriptorState.dirtyViews(buffer->getShaderStages());

    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    
    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    
    if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
      m_flags.set(DxvkContextFlag::DirtyDrawBuffer);

    if (usage & VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT)
      m_flags.set(DxvkContextFlag::GpDirtyXfbBuffers);
  }


  void DxvkContext::ensureBufferAddress(
    const Rc<DxvkBuffer>&           buffer) {
    // Really nothing else to do here but set the flag
    buffer->enableStableAddress();
  }


  void DxvkContext::invalidateImage(
    const Rc<DxvkImage>&            image,
          Rc<DxvkResourceAllocation>&& slice) {
    // Ensure image is in the correct layout and not currently tracked
    prepareImage(image, image->getAvailableSubresources());

    invalidateImageWithUsage(image, std::move(slice), DxvkImageUsageInfo());
  }


  void DxvkContext::invalidateImageWithUsage(
    const Rc<DxvkImage>&            image,
          Rc<DxvkResourceAllocation>&& slice,
    const DxvkImageUsageInfo&       usageInfo) {
    Rc<DxvkResourceAllocation> prevAllocation = image->assignResourceWithUsage(std::move(slice), usageInfo);
    m_cmd->trackResource<DxvkAccess::None>(std::move(prevAllocation));

    VkImageUsageFlags usage = image->info().usage;

    if (usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT))
      m_descriptorState.dirtyViews(image->getShaderStages());

    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      // Interrupt the current render pass if the image is bound for rendering
      for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
        if (m_state.om.framebufferInfo.getAttachment(i).view->image() == image) {
          this->spillRenderPass(false);

          m_flags.set(DxvkContextFlag::GpDirtyFramebuffer);
          break;
        }
      }
    }
  }


  bool DxvkContext::ensureImageCompatibility(
    const Rc<DxvkImage>&            image,
    const DxvkImageUsageInfo&       usageInfo) {
    // Check whether image usage, flags and view formats are supported.
    // If any of these are false, we need to create a new image.
    bool isUsageAndFormatCompatible = (image->info().usage & usageInfo.usage) == usageInfo.usage
                                   && (image->info().flags & usageInfo.flags) == usageInfo.flags;

    for (uint32_t i = 0; i < usageInfo.viewFormatCount && isUsageAndFormatCompatible; i++)
      isUsageAndFormatCompatible &= image->isViewCompatible(usageInfo.viewFormats[i]);

    // Check if we need to insert a barrier and update image properties
    bool isAccessAndLayoutCompatible = (image->info().stages & usageInfo.stages) == usageInfo.stages
                                    && (image->info().access & usageInfo.access) == usageInfo.access
                                    && (usageInfo.layout && image->info().layout == usageInfo.layout);

    // If everything matches already, no need to do anything. Only ensure
    // that the stable adress bit is respected if set for the first time.
    if (isUsageAndFormatCompatible && isAccessAndLayoutCompatible) {
      if (usageInfo.stableGpuAddress && image->canRelocate())
        image->assignResourceWithUsage(image->getAllocation(), usageInfo);

      return true;
    }

    // Ensure the image is accessible and in its default layout
    this->spillRenderPass(true);
    this->prepareImage(image, image->getAvailableSubresources());

    if (m_execBarriers.isImageDirty(image, image->getAvailableSubresources(), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    if (isUsageAndFormatCompatible) {
      // Emit a barrier. If used in internal passes, this function
      // must be called *before* emitting dirty checks there.
      VkImageLayout oldLayout = image->info().layout;
      VkImageLayout newLayout = usageInfo.layout ? usageInfo.layout : oldLayout;

      image->assignResourceWithUsage(image->getAllocation(), usageInfo);

      m_execBarriers.accessImage(image, image->getAvailableSubresources(),
        oldLayout, image->info().stages, image->info().access,
        newLayout, image->info().stages, image->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(image);
      return true;
    }

    // Some images have to stay in their place, we can't do much in that case.
    if (!image->canRelocate()) {
      Logger::err(str::format("DxvkContext: Cannot relocate image:",
        "\n  Current usage:   0x", std::hex, image->info().usage, ", flags: 0x", image->info().flags, ", ", std::dec, image->info().viewFormatCount, " view formats"
        "\n  Requested usage: 0x", std::hex, usageInfo.usage, ", flags: 0x", usageInfo.flags, ", ", std::dec, usageInfo.viewFormatCount, " view formats"));
      return false;
    }

    // Enable mutable format bit as necessary. We do not require
    // setting this explicitly so that the caller does not have
    // to check image formats every time.
    VkImageCreateFlags createFlags = 0u;

    for (uint32_t i = 0; i < usageInfo.viewFormatCount; i++) {
      if (usageInfo.viewFormats[i] != image->info().format) {
        createFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        break;
      }
    }

    // Ensure that the image can support the requested usage
    VkFormatFeatureFlagBits2 required = 0u;

    switch (usageInfo.usage) {
      case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
        required |= VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;
        break;

      case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
        required |= VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;

      case VK_IMAGE_USAGE_SAMPLED_BIT:
        required |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;
        break;

      case VK_IMAGE_USAGE_STORAGE_BIT:
        required |= VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT;
        break;

      case VK_IMAGE_USAGE_TRANSFER_SRC_BIT:
        required |= VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT;
        break;

      case VK_IMAGE_USAGE_TRANSFER_DST_BIT:
        required |= VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT;
        break;

      default:
        break;
    }

    // Make sure to use the correct set of feature flags for this image
    auto features = m_device->getFormatFeatures(image->info().format);
    auto& supported = image->info().tiling == VK_IMAGE_TILING_OPTIMAL
      ? features.optimal : features.linear;

    if ((supported & required) != required) {
      // Check if any of the view formats support the required features
      for (uint32_t i = 0; i < image->info().viewFormatCount; i++) {
        auto extendedFeatures = m_device->getFormatFeatures(image->info().viewFormats[i]);
        features.optimal |= extendedFeatures.optimal;
        features.linear |= extendedFeatures.linear;
      }

      for (uint32_t i = 0; i < usageInfo.viewFormatCount; i++) {
        auto extendedFeatures = m_device->getFormatFeatures(usageInfo.viewFormats[i]);
        features.optimal |= extendedFeatures.optimal;
        features.linear |= extendedFeatures.linear;
      }

      if ((supported & required) != required)
        return false;

      // We're good, just need to enable extended usage
      createFlags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    }

    // Allocate new backing storage and relocate the image
    DxvkImageUsageInfo usage = usageInfo;
    usage.flags |= createFlags;

    auto storage = image->createResourceWithUsage(usage);

    DxvkRelocateImageInfo relocateInfo;
    relocateInfo.image = image;
    relocateInfo.storage = storage;
    relocateInfo.usageInfo = usage;

    relocateResources(0, nullptr, 1, &relocateInfo);
    return true;
  }


  void DxvkContext::resolveImage(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format) {
    this->spillRenderPass(true);
    this->prepareImage(dstImage, vk::makeSubresourceRange(region.dstSubresource));
    this->prepareImage(srcImage, vk::makeSubresourceRange(region.srcSubresource));

    if (format == VK_FORMAT_UNDEFINED)
      format = srcImage->info().format;

    bool useFb = srcImage->info().format != format
              || dstImage->info().format != format;

    if (m_device->perfHints().preferFbResolve) {
      useFb |= (dstImage->info().usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            && (srcImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    if (!useFb) {
      this->resolveImageHw(
        dstImage, srcImage, region);
    } else {
      this->resolveImageFb(
        dstImage, srcImage, region, format,
        VK_RESOLVE_MODE_NONE,
        VK_RESOLVE_MODE_NONE);
    }
  }


  void DxvkContext::resolveDepthStencilImage(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkResolveModeFlagBits     depthMode,
          VkResolveModeFlagBits     stencilMode) {
    this->spillRenderPass(true);
    this->prepareImage(dstImage, vk::makeSubresourceRange(region.dstSubresource));
    this->prepareImage(srcImage, vk::makeSubresourceRange(region.srcSubresource));

    // Technically legal, but no-op
    if (!depthMode && !stencilMode)
      return;

    // Subsequent functions expect stencil mode to be None
    // if either of the images have no stencil aspect
    if (!(region.dstSubresource.aspectMask
        & region.srcSubresource.aspectMask
        & VK_IMAGE_ASPECT_STENCIL_BIT))
      stencilMode = VK_RESOLVE_MODE_NONE;

    // We can only use the depth-stencil resolve path if we are resolving
    // a full subresource and both images have the same format.
    bool useFb = !dstImage->isFullSubresource(region.dstSubresource, region.extent)
              || !srcImage->isFullSubresource(region.srcSubresource, region.extent)
              || dstImage->info().format != srcImage->info().format;
    
    if (!useFb) {
      // Additionally, the given mode combination must be supported.
      const auto& properties = m_device->properties().vk12;

      useFb |= (properties.supportedDepthResolveModes   & depthMode)   != depthMode
            || (properties.supportedStencilResolveModes & stencilMode) != stencilMode;
      
      if (depthMode != stencilMode) {
        useFb |= (!depthMode || !stencilMode)
          ? !properties.independentResolveNone
          : !properties.independentResolve;
      }
    }

    // If the source image is shader-readable anyway, we can use the
    // FB path if it's beneficial on the device we're running on
    if (m_device->perfHints().preferFbDepthStencilCopy)
      useFb |= srcImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT;

    if (useFb) {
      this->resolveImageFb(
        dstImage, srcImage, region, VK_FORMAT_UNDEFINED,
        depthMode, stencilMode);
    } else {
      this->resolveImageDs(
        dstImage, srcImage, region,
        depthMode, stencilMode);
    }
  }


  void DxvkContext::transformImage(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceRange&  dstSubresources,
          VkImageLayout             srcLayout,
          VkImageLayout             dstLayout) {
    this->spillRenderPass(false);
    
    if (srcLayout != dstLayout) {
      m_execBarriers.recordCommands(m_cmd);

      m_execBarriers.accessImage(
        dstImage, dstSubresources,
        srcLayout,
        dstImage->info().stages,
        dstImage->info().access,
        dstLayout,
        dstImage->info().stages,
        dstImage->info().access);
      
      m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    }
  }
  
  
  void DxvkContext::performClear(
    const Rc<DxvkImageView>&        imageView,
          int32_t                   attachmentIndex,
          VkImageAspectFlags        discardAspects,
          VkImageAspectFlags        clearAspects,
          VkClearValue              clearValue) {
    DxvkColorAttachmentOps colorOp;
    colorOp.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorOp.loadLayout    = imageView->image()->info().layout;
    colorOp.storeLayout   = imageView->image()->info().layout;
    
    DxvkDepthAttachmentOps depthOp;
    depthOp.loadOpD       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthOp.loadOpS       = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthOp.loadLayout    = imageView->image()->info().layout;
    depthOp.storeLayout   = imageView->image()->info().layout;

    if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    
    if (clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    
    if (clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    if (attachmentIndex >= 0 && !m_state.om.framebufferInfo.isWritable(attachmentIndex, clearAspects | discardAspects)) {
      // Do not fold the clear/discard into the render pass if any of the affected aspects
      // isn't writable. We can only hit this particular path when starting a render pass,
      // so we can safely manipulate load layouts here.
      int32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);
      VkImageLayout renderLayout = m_state.om.framebufferInfo.getAttachment(attachmentIndex).layout;

      if (colorIndex < 0) {
        depthOp.loadLayout = m_state.om.renderPassOps.depthOps.loadLayout;
        depthOp.storeLayout = renderLayout;
        m_state.om.renderPassOps.depthOps.loadLayout = renderLayout;
      } else {
        colorOp.loadLayout = m_state.om.renderPassOps.colorOps[colorIndex].loadLayout;
        colorOp.storeLayout = renderLayout;
        m_state.om.renderPassOps.colorOps[colorIndex].loadLayout = renderLayout;
      }

      attachmentIndex = -1;
    }

    bool is3D = imageView->image()->info().type == VK_IMAGE_TYPE_3D;

    if ((clearAspects | discardAspects) == imageView->info().aspects && !is3D) {
      colorOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      depthOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    if (attachmentIndex < 0) {
      bool hasViewFormatMismatch = imageView->info().format != imageView->image()->info().format;

      if (m_execBarriers.isImageDirty(imageView->image(), imageView->imageSubresources(), DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);

      // Set up a temporary render pass to execute the clear
      VkImageLayout imageLayout = ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_COLOR_BIT)
        ? imageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        : imageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

      VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
      attachmentInfo.imageView = imageView->handle();
      attachmentInfo.imageLayout = imageLayout;
      attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentInfo.clearValue = clearValue;

      VkRenderingAttachmentInfo stencilInfo = attachmentInfo;

      VkExtent3D extent = imageView->mipLevelExtent(0);

      VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
      renderingInfo.renderArea.extent = { extent.width, extent.height };
      renderingInfo.layerCount = imageView->info().layerCount;

      VkImageLayout loadLayout;
      VkImageLayout storeLayout;

      VkPipelineStageFlags clearStages = 0;
      VkAccessFlags        clearAccess = 0;
      
      if ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_COLOR_BIT) {
        clearStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        clearAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        attachmentInfo.loadOp = colorOp.loadOp;

        // We can't use LOAD_OP_CLEAR if the view format does not match the
        // underlying image format, so just discard here and use clear later.
        if (hasViewFormatMismatch && attachmentInfo.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
          attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;

        loadLayout = colorOp.loadLayout;
        storeLayout = colorOp.storeLayout;
      } else {
        clearStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        clearAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        if (imageView->info().aspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
          renderingInfo.pDepthAttachment = &attachmentInfo;
          attachmentInfo.loadOp = depthOp.loadOpD;
        }

        if (imageView->info().aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
          renderingInfo.pStencilAttachment = &stencilInfo;
          stencilInfo.loadOp = depthOp.loadOpS;
        }

        loadLayout = depthOp.loadLayout;
        storeLayout = depthOp.storeLayout;
      }

      if (loadLayout != imageLayout) {
        m_execAcquires.accessImage(
          imageView->image(),
          imageView->imageSubresources(),
          loadLayout, clearStages, 0,
          imageLayout, clearStages, clearAccess);

        m_execAcquires.recordCommands(m_cmd);
      }

      m_cmd->cmdBeginRendering(&renderingInfo);

      if (hasViewFormatMismatch) {
        VkClearAttachment clearInfo = { };
        clearInfo.aspectMask = imageView->info().aspects;
        clearInfo.clearValue = clearValue;

        VkClearRect clearRect = { };
        clearRect.rect.extent.width = extent.width;
        clearRect.rect.extent.height = extent.height;
        clearRect.layerCount = imageView->info().layerCount;

        m_cmd->cmdClearAttachments(1, &clearInfo, 1, &clearRect);
      }

      m_cmd->cmdEndRendering();

      m_execBarriers.accessImage(
        imageView->image(),
        imageView->imageSubresources(),
        imageLayout, clearStages, clearAccess,
        storeLayout,
        imageView->image()->info().stages,
        imageView->image()->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(imageView->image());
    } else {
      // Perform the operation when starting the next render pass
      if ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_COLOR_BIT) {
        uint32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

        m_state.om.renderPassOps.colorOps[colorIndex].loadOp = colorOp.loadOp;
        if (m_state.om.renderPassOps.colorOps[colorIndex].loadOp != VK_ATTACHMENT_LOAD_OP_LOAD && !is3D)
          m_state.om.renderPassOps.colorOps[colorIndex].loadLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        m_state.om.renderPassOps.colorOps[colorIndex].clearValue = clearValue.color;
      }
      
      if ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_DEPTH_BIT) {
        m_state.om.renderPassOps.depthOps.loadOpD = depthOp.loadOpD;
        m_state.om.renderPassOps.depthOps.clearValue.depth = clearValue.depthStencil.depth;
      }
      
      if ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_STENCIL_BIT) {
        m_state.om.renderPassOps.depthOps.loadOpS = depthOp.loadOpS;
        m_state.om.renderPassOps.depthOps.clearValue.stencil = clearValue.depthStencil.stencil;
      }

      if ((clearAspects | discardAspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        if (m_state.om.renderPassOps.depthOps.loadOpD != VK_ATTACHMENT_LOAD_OP_LOAD
         && m_state.om.renderPassOps.depthOps.loadOpS != VK_ATTACHMENT_LOAD_OP_LOAD)
          m_state.om.renderPassOps.depthOps.loadLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      }
    }
  }


  void DxvkContext::deferClear(
    const Rc<DxvkImageView>&        imageView,
          VkImageAspectFlags        clearAspects,
          VkClearValue              clearValue) {
    for (auto& entry : m_deferredClears) {
      if (entry.imageView->matchesView(imageView)) {
        entry.imageView = imageView;
        entry.discardAspects &= ~clearAspects;
        entry.clearAspects |= clearAspects;

        if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
          entry.clearValue.color = clearValue.color;
        if (clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
          entry.clearValue.depthStencil.depth = clearValue.depthStencil.depth;
        if (clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
          entry.clearValue.depthStencil.stencil = clearValue.depthStencil.stencil;
        
        return;
      } else if (entry.imageView->checkSubresourceOverlap(imageView)) {
        this->spillRenderPass(false);
        break;
      }
    }

    m_deferredClears.push_back({ imageView, 0, clearAspects, clearValue });
  }


  void DxvkContext::deferDiscard(
    const Rc<DxvkImageView>&        imageView,
          VkImageAspectFlags        discardAspects) {
    for (auto& entry : m_deferredClears) {
      if (entry.imageView->matchesView(imageView)) {
        entry.imageView = imageView;
        entry.discardAspects |= discardAspects;
        entry.clearAspects &= ~discardAspects;
        return;
      } else if (entry.imageView->checkSubresourceOverlap(imageView)) {
        this->spillRenderPass(false);
        break;
      }
    }

    m_deferredClears.push_back({ imageView, discardAspects });
  }


  void DxvkContext::flushClears(
          bool                      useRenderPass) {
    for (const auto& clear : m_deferredClears) {
      int32_t attachmentIndex = -1;

      if (useRenderPass && m_state.om.framebufferInfo.isFullSize(clear.imageView))
        attachmentIndex = m_state.om.framebufferInfo.findAttachment(clear.imageView);

      this->performClear(clear.imageView, attachmentIndex,
        clear.discardAspects, clear.clearAspects, clear.clearValue);
    }

    m_deferredClears.clear();
  }


  void DxvkContext::flushSharedImages() {
    for (auto i = m_deferredClears.begin(); i != m_deferredClears.end(); ) {
      if (i->imageView->image()->info().shared) {
        this->performClear(i->imageView, -1, i->discardAspects, i->clearAspects, i->clearValue);
        i = m_deferredClears.erase(i);
      } else {
        i++;
      }
    }

    this->transitionRenderTargetLayouts(true);
  }


  void DxvkContext::updateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
    const void*                     data) {
    bool replaceBuffer = this->tryInvalidateDeviceLocalBuffer(buffer, size);
    auto bufferSlice = buffer->getSliceHandle(offset, size);

    if (!replaceBuffer) {
      this->spillRenderPass(true);
    
      if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);
    }

    DxvkCmdBuffer cmdBuffer = replaceBuffer
      ? DxvkCmdBuffer::InitBuffer
      : DxvkCmdBuffer::ExecBuffer;

    m_cmd->cmdUpdateBuffer(cmdBuffer,
      bufferSlice.handle,
      bufferSlice.offset,
      bufferSlice.length,
      data);

    auto& barriers = replaceBuffer
      ? m_initBarriers
      : m_execBarriers;

    barriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(buffer);
  }
  
  
  void DxvkContext::uploadBuffer(
    const Rc<DxvkBuffer>&           buffer,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset) {
    auto bufferSlice = buffer->getSliceHandle();
    auto sourceSlice = source->getSliceHandle(sourceOffset, buffer->info().size);

    VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copyRegion.srcOffset = sourceSlice.offset;
    copyRegion.dstOffset = bufferSlice.offset;
    copyRegion.size      = bufferSlice.length;

    VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo.srcBuffer = sourceSlice.handle;
    copyInfo.dstBuffer = bufferSlice.handle;
    copyInfo.regionCount = 1;
    copyInfo.pRegions = &copyRegion;

    m_cmd->cmdCopyBuffer(DxvkCmdBuffer::SdmaBuffer, &copyInfo);

    if (m_device->hasDedicatedTransferQueue()) {
      // Buffers use SHARING_MODE_CONCURRENT, so no explicit queue
      // family ownership transfer is required. Access is serialized
      // via a semaphore.
      m_sdmaBarriers.accessMemory(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_NONE, VK_ACCESS_NONE);

      m_initBarriers.accessMemory(
        VK_PIPELINE_STAGE_NONE, VK_ACCESS_NONE,
        buffer->info().stages, buffer->info().access);
    } else {
      m_sdmaBarriers.accessMemory(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        buffer->info().stages, buffer->info().access);
    }

    m_cmd->trackResource<DxvkAccess::Read>(source);
    m_cmd->trackResource<DxvkAccess::Write>(buffer);
  }


  void DxvkContext::uploadImage(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset,
          VkFormat                  format) {
    // Always use framebuffer path for depth-stencil images since we know
    // they are writeable and can't use Vulkan transfer queues. Stencil
    // data is interleaved and needs to be decoded manually anyway.
    bool useFb = !formatsAreCopyCompatible(image->info().format, format);

    if (useFb)
      uploadImageFb(image, source, sourceOffset, format);
    else
      uploadImageHw(image, source, sourceOffset);
  }


  void DxvkContext::setViewports(
          uint32_t            viewportCount,
    const VkViewport*         viewports,
    const VkRect2D*           scissorRects) {
    for (uint32_t i = 0; i < viewportCount; i++) {
      m_state.vp.viewports[i] = viewports[i];
      m_state.vp.scissorRects[i] = scissorRects[i];
      
      // Vulkan viewports are not allowed to have a width or
      // height of zero, so we fall back to a dummy viewport
      // and instead set an empty scissor rect, which is legal.
      if (viewports[i].width == 0.0f || viewports[i].height == 0.0f) {
        m_state.vp.viewports[i] = VkViewport {
          0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
        m_state.vp.scissorRects[i] = VkRect2D {
          VkOffset2D { 0, 0 },
          VkExtent2D { 0, 0 } };
      }
    }
    
    m_state.vp.viewportCount = viewportCount;
    m_flags.set(DxvkContextFlag::GpDirtyViewport);
  }
  
  
  void DxvkContext::setBlendConstants(
          DxvkBlendConstants  blendConstants) {
    if (m_state.dyn.blendConstants != blendConstants) {
      m_state.dyn.blendConstants = blendConstants;
      m_flags.set(DxvkContextFlag::GpDirtyBlendConstants);
    }
  }
  
  
  void DxvkContext::setDepthBias(
          DxvkDepthBias       depthBias) {
    if (m_state.dyn.depthBias != depthBias) {
      m_state.dyn.depthBias = depthBias;
      m_flags.set(DxvkContextFlag::GpDirtyDepthBias);
    }
  }


  void DxvkContext::setDepthBiasRepresentation(
          DxvkDepthBiasRepresentation  depthBiasRepresentation) {
    if (m_state.dyn.depthBiasRepresentation != depthBiasRepresentation) {
      m_state.dyn.depthBiasRepresentation = depthBiasRepresentation;
      m_flags.set(DxvkContextFlag::GpDirtyDepthBias);
    }
  }


  void DxvkContext::setDepthBounds(
          DxvkDepthBounds     depthBounds) {
    if (m_state.dyn.depthBounds != depthBounds) {
      m_state.dyn.depthBounds = depthBounds;
      m_flags.set(DxvkContextFlag::GpDirtyDepthBounds);
    }

    if (m_state.gp.state.ds.enableDepthBoundsTest() != depthBounds.enableDepthBounds) {
      m_state.gp.state.ds.setEnableDepthBoundsTest(depthBounds.enableDepthBounds);
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
  }
  
  
  void DxvkContext::setStencilReference(
          uint32_t            reference) {
    if (m_state.dyn.stencilReference != reference) {
      m_state.dyn.stencilReference = reference;
      m_flags.set(DxvkContextFlag::GpDirtyStencilRef);
    }
  }
  
  
  void DxvkContext::setInputAssemblyState(const DxvkInputAssemblyState& ia) {
    m_state.gp.state.ia = DxvkIaInfo(
      ia.primitiveTopology,
      ia.primitiveRestart,
      ia.patchVertexCount);
    
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

    for (uint32_t i = 0; i < bindingCount; i++) {
      m_state.gp.state.ilBindings[i] = DxvkIlBinding(
        bindings[i].binding, 0, bindings[i].inputRate,
        bindings[i].fetchRate);
      m_state.vi.vertexExtents[i] = bindings[i].extent;
    }

    for (uint32_t i = bindingCount; i < m_state.gp.state.il.bindingCount(); i++) {
      m_state.gp.state.ilBindings[i] = DxvkIlBinding();
      m_state.vi.vertexExtents[i] = 0;
    }
    
    for (uint32_t i = 0; i < attributeCount; i++) {
      m_state.gp.state.ilAttributes[i] = DxvkIlAttribute(
        attributes[i].location, attributes[i].binding,
        attributes[i].format,   attributes[i].offset);
    }
    
    for (uint32_t i = attributeCount; i < m_state.gp.state.il.attributeCount(); i++)
      m_state.gp.state.ilAttributes[i] = DxvkIlAttribute();
    
    m_state.gp.state.il = DxvkIlInfo(attributeCount, bindingCount);
  }
  
  
  void DxvkContext::setRasterizerState(const DxvkRasterizerState& rs) {
    if (m_state.dyn.cullMode != rs.cullMode || m_state.dyn.frontFace != rs.frontFace) {
      m_state.dyn.cullMode = rs.cullMode;
      m_state.dyn.frontFace = rs.frontFace;

      m_flags.set(DxvkContextFlag::GpDirtyRasterizerState);
    }

    if (unlikely(rs.sampleCount != m_state.gp.state.rs.sampleCount())) {
      if (!m_state.gp.state.ms.sampleCount())
        m_flags.set(DxvkContextFlag::GpDirtyMultisampleState);

      if (!m_features.test(DxvkContextFeature::VariableMultisampleRate))
        m_flags.set(DxvkContextFlag::GpDirtyFramebuffer);
    }

    DxvkRsInfo rsInfo(
      rs.depthClipEnable,
      rs.depthBiasEnable,
      rs.polygonMode,
      rs.sampleCount,
      rs.conservativeMode,
      rs.flatShading,
      rs.lineMode);

    if (!m_state.gp.state.rs.eq(rsInfo)) {
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);

      // Since depth bias enable is only dynamic for base pipelines,
      // it is applied as part of the dynamic depth-stencil state
      if (m_state.gp.state.rs.depthBiasEnable() != rs.depthBiasEnable)
        m_flags.set(DxvkContextFlag::GpDirtyDepthStencilState);

      m_state.gp.state.rs = rsInfo;
    }
  }
  
  
  void DxvkContext::setMultisampleState(const DxvkMultisampleState& ms) {
    m_state.gp.state.ms = DxvkMsInfo(
      m_state.gp.state.ms.sampleCount(),
      ms.sampleMask,
      ms.enableAlphaToCoverage);
    
    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyMultisampleState);
  }
  
  
  void DxvkContext::setDepthStencilState(const DxvkDepthStencilState& ds) {
    m_state.gp.state.ds = DxvkDsInfo(
      ds.enableDepthTest,
      ds.enableDepthWrite,
      m_state.gp.state.ds.enableDepthBoundsTest(),
      ds.enableStencilTest,
      ds.depthCompareOp);
    
    m_state.gp.state.dsFront = DxvkDsStencilOp(ds.stencilOpFront);
    m_state.gp.state.dsBack  = DxvkDsStencilOp(ds.stencilOpBack);
    
    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyDepthStencilState);
  }
  
  
  void DxvkContext::setLogicOpState(const DxvkLogicOpState& lo) {
    m_state.gp.state.om = DxvkOmInfo(
      lo.enableLogicOp,
      lo.logicOp,
      m_state.gp.state.om.feedbackLoop());
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setBlendMode(
          uint32_t            attachment,
    const DxvkBlendMode&      blendMode) {
    m_state.gp.state.omBlend[attachment] = DxvkOmAttachmentBlend(
      blendMode.enableBlending,
      blendMode.colorSrcFactor,
      blendMode.colorDstFactor,
      blendMode.colorBlendOp,
      blendMode.alphaSrcFactor,
      blendMode.alphaDstFactor,
      blendMode.alphaBlendOp,
      blendMode.writeMask);
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }


  void DxvkContext::setBarrierControl(DxvkBarrierControlFlags control) {
    m_barrierControl = control;
  }
  
  
  void DxvkContext::updatePageTable(
    const DxvkSparseBindInfo&   bindInfo,
          DxvkSparseBindFlags   flags) {
    // Split command buffers here so that we execute
    // the sparse binding operation at the right time
    if (!flags.test(DxvkSparseBindFlag::SkipSynchronization))
      this->splitCommands();

    DxvkSparsePageAllocator* srcAllocator = bindInfo.srcAllocator.ptr();
    DxvkSparsePageTable* dstPageTable = bindInfo.dstResource->getSparsePageTable();
    DxvkSparsePageTable* srcPageTable = nullptr;

    if (bindInfo.srcResource != nullptr)
      srcPageTable = bindInfo.srcResource->getSparsePageTable();

    // In order to support copies properly, we need to buffer the new
    // mappings first before we apply them to the destination resource.
    size_t bindCount = bindInfo.binds.size();
    std::vector<DxvkSparseMapping> mappings(bindCount);

    for (size_t i = 0; i < bindCount; i++) {
      DxvkSparseBind bind = bindInfo.binds[i];

      switch (bind.mode) {
        case DxvkSparseBindMode::Null:
          // The mapping array is already default-initialized
          // so we don't actually need to do anything here
          break;

        case DxvkSparseBindMode::Bind:
          mappings[i] = srcAllocator->acquirePage(bind.srcPage);
          break;

        case DxvkSparseBindMode::Copy:
          mappings[i] = srcPageTable->getMapping(bind.srcPage);
          break;
      }
    }

    // Process the actual page table updates here and resolve
    // our internal structures to Vulkan resource and memory
    // handles. The rest will be done at submission time.
    for (size_t i = 0; i < bindCount; i++) {
      DxvkSparseBind bind = bindInfo.binds[i];
      DxvkSparseMapping mapping = std::move(mappings[i]);

      DxvkSparsePageInfo pageInfo = dstPageTable->getPageInfo(bind.dstPage);

      switch (pageInfo.type) {
        case DxvkSparsePageType::None:
          break;

        case DxvkSparsePageType::Buffer: {
          DxvkSparseBufferBindKey key;
          key.buffer = dstPageTable->getBufferHandle();
          key.offset = pageInfo.buffer.offset;
          key.size   = pageInfo.buffer.length;

          m_cmd->bindBufferMemory(key, mapping.getMemoryInfo());
        } break;

        case DxvkSparsePageType::Image: {
          DxvkSparseImageBindKey key;
          key.image = dstPageTable->getImageHandle();
          key.subresource = pageInfo.image.subresource;
          key.offset = pageInfo.image.offset;
          key.extent = pageInfo.image.extent;

          m_cmd->bindImageMemory(key, mapping.getMemoryInfo());
        } break;

        case DxvkSparsePageType::ImageMipTail: {
          DxvkSparseImageOpaqueBindKey key;
          key.image  = dstPageTable->getImageHandle();
          key.offset = pageInfo.mipTail.resourceOffset;
          key.size   = pageInfo.mipTail.resourceLength;
          key.flags  = 0;

          m_cmd->bindImageOpaqueMemory(key, mapping.getMemoryInfo());
        } break;
      }

      // Update the page table mapping for tracking purposes
      if (pageInfo.type != DxvkSparsePageType::None)
        dstPageTable->updateMapping(m_cmd.ptr(), bind.dstPage, std::move(mapping));
    }

    m_cmd->trackResource<DxvkAccess::Write>(bindInfo.dstResource);
  }


  void DxvkContext::signalGpuEvent(const Rc<DxvkGpuEvent>& event) {
    this->spillRenderPass(true);
    
    DxvkGpuEventHandle handle = m_common->eventPool().allocEvent();

    // Supported client APIs can't access device memory in a defined manner
    // without triggering a queue submission first, so we really only need
    // to wait for prior commands, especially queries, to complete.
    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    m_cmd->cmdSetEvent(handle.event, &depInfo);

    m_cmd->trackGpuEvent(event->reset(handle));
    m_cmd->trackResource<DxvkAccess::None>(event);
  }
  

  void DxvkContext::launchCuKernelNVX(
    const VkCuLaunchInfoNVX& nvxLaunchInfo,
    const std::vector<std::pair<Rc<DxvkBuffer>, DxvkAccessFlags>>& buffers,
    const std::vector<std::pair<Rc<DxvkImage>,  DxvkAccessFlags>>& images) {
    // The resources in the std::vectors above are called-out
    // explicitly in the API for barrier and tracking purposes
    // since they're being used bindlessly.
    this->spillRenderPass(true);

    VkPipelineStageFlags srcStages = 0;
    VkAccessFlags srcAccess = 0;

    for (auto& r : buffers) {
      srcStages |= r.first->info().stages;
      srcAccess |= r.first->info().access;
    }

    for (auto& r : images) {
      srcStages |= r.first->info().stages;
      srcAccess |= r.first->info().access;

      this->prepareImage(r.first, r.first->getAvailableSubresources());
    }

    m_execBarriers.accessMemory(srcStages, srcAccess,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    m_execBarriers.recordCommands(m_cmd);

    m_cmd->cmdLaunchCuKernel(nvxLaunchInfo);

    for (auto& r : buffers) {
      VkAccessFlags accessFlags = (r.second.test(DxvkAccess::Read) * VK_ACCESS_SHADER_READ_BIT)
                                | (r.second.test(DxvkAccess::Write) * VK_ACCESS_SHADER_WRITE_BIT);
      DxvkBufferSliceHandle bufferSlice = r.first->getSliceHandle();
      m_execBarriers.accessBuffer(bufferSlice,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        accessFlags,
        r.first->info().stages,
        r.first->info().access);
    }

    for (auto& r : images) {
      VkAccessFlags accessFlags = (r.second.test(DxvkAccess::Read) * VK_ACCESS_SHADER_READ_BIT)
                                | (r.second.test(DxvkAccess::Write) * VK_ACCESS_SHADER_WRITE_BIT);
      m_execBarriers.accessImage(r.first,
        r.first->getAvailableSubresources(),
        r.first->info().layout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        accessFlags,
        r.first->info().layout,
        r.first->info().stages,
        r.first->info().access);
    }

    for (auto& r : images) {
      if (r.second.test(DxvkAccess::Read)) m_cmd->trackResource<DxvkAccess::Read>(r.first);
      if (r.second.test(DxvkAccess::Write)) m_cmd->trackResource<DxvkAccess::Write>(r.first);
    }

    for (auto& r : buffers) {
      if (r.second.test(DxvkAccess::Read)) m_cmd->trackResource<DxvkAccess::Read>(r.first);
      if (r.second.test(DxvkAccess::Write)) m_cmd->trackResource<DxvkAccess::Write>(r.first);
    }
  }
  
  
  void DxvkContext::writeTimestamp(const Rc<DxvkGpuQuery>& query) {
    m_queryManager.writeTimestamp(m_cmd, query);
  }


  void DxvkContext::signal(const Rc<sync::Signal>& signal, uint64_t value) {
    m_cmd->queueSignal(signal, value);
  }


  void DxvkContext::waitFence(const Rc<DxvkFence>& fence, uint64_t value) {
    m_cmd->waitFence(fence, value);
  }


  void DxvkContext::signalFence(const Rc<DxvkFence>& fence, uint64_t value) {
    m_cmd->signalFence(fence, value);
  }


  void DxvkContext::beginDebugLabel(VkDebugUtilsLabelEXT *label) {
    if (!m_device->instance()->extensions().extDebugUtils)
      return;

    m_cmd->cmdBeginDebugUtilsLabel(label);
  }

  void DxvkContext::endDebugLabel() {
    if (!m_device->instance()->extensions().extDebugUtils)
      return;

    m_cmd->cmdEndDebugUtilsLabel();
  }

  void DxvkContext::insertDebugLabel(VkDebugUtilsLabelEXT *label) {
    if (!m_device->instance()->extensions().extDebugUtils)
      return;

    m_cmd->cmdInsertDebugUtilsLabel(label);
  }
  
  
  void DxvkContext::blitImageFb(
          Rc<DxvkImageView>     dstView,
    const VkOffset3D*           dstOffsets,
          Rc<DxvkImageView>     srcView,
    const VkOffset3D*           srcOffsets,
          VkFilter              filter) {
    this->invalidateState();

    bool srcIsDepthStencil = srcView->info().aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    bool dstIsDepthStencil = dstView->info().aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    dstView = ensureImageViewCompatibility(dstView, dstIsDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    srcView = ensureImageViewCompatibility(srcView, VK_IMAGE_USAGE_SAMPLED_BIT);

    if (!dstView || !srcView) {
      Logger::err(str::format("DxvkContext: blitImageFb: Resources not supported"));
      return;
    }

    if (m_execBarriers.isImageDirty(dstView->image(), dstView->imageSubresources(), DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcView->image(), srcView->imageSubresources(), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    VkImageLayout srcLayout = srcView->image()->pickLayout(srcIsDepthStencil
      ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkImageLayout dstLayout = dstView->image()->pickLayout(
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (dstView->image()->info().layout != dstLayout) {
      m_execAcquires.accessImage(
        dstView->image(),
        dstView->imageSubresources(),
        dstView->image()->info().layout,
        dstView->image()->info().stages, 0,
        dstLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }

    if (srcView->image()->info().layout != srcLayout) {
      m_execAcquires.accessImage(
        srcView->image(),
        srcView->imageSubresources(),
        srcView->image()->info().layout,
        srcView->image()->info().stages, 0,
        srcLayout,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);

    // Sort out image offsets so that dstOffset[0] points
    // to the top-left corner of the target area
    std::array<VkOffset3D, 2> srcOffsetsAdjusted = { srcOffsets[0], srcOffsets[1] };
    std::array<VkOffset3D, 2> dstOffsetsAdjusted = { dstOffsets[0], dstOffsets[1] };

    if (dstOffsetsAdjusted[0].x > dstOffsetsAdjusted[1].x) {
      std::swap(dstOffsetsAdjusted[0].x, dstOffsetsAdjusted[1].x);
      std::swap(srcOffsetsAdjusted[0].x, srcOffsetsAdjusted[1].x);
    }

    if (dstOffsetsAdjusted[0].y > dstOffsetsAdjusted[1].y) {
      std::swap(dstOffsetsAdjusted[0].y, dstOffsetsAdjusted[1].y);
      std::swap(srcOffsetsAdjusted[0].y, srcOffsetsAdjusted[1].y);
    }

    if (dstOffsetsAdjusted[0].z > dstOffsetsAdjusted[1].z) {
      std::swap(dstOffsetsAdjusted[0].z, dstOffsetsAdjusted[1].z);
      std::swap(srcOffsetsAdjusted[0].z, srcOffsetsAdjusted[1].z);
    }
    
    VkExtent3D dstExtent = {
      uint32_t(dstOffsetsAdjusted[1].x - dstOffsetsAdjusted[0].x),
      uint32_t(dstOffsetsAdjusted[1].y - dstOffsetsAdjusted[0].y),
      uint32_t(dstOffsetsAdjusted[1].z - dstOffsetsAdjusted[0].z) };

    // Begin render pass
    VkExtent3D imageExtent = dstView->mipLevelExtent(0);

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = dstView->handle();
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea = VkRect2D {
      VkOffset2D { 0, 0 },
      VkExtent2D { imageExtent.width, imageExtent.height } };
    renderingInfo.layerCount = dstView->info().layerCount;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    m_cmd->cmdBeginRendering(&renderingInfo);

    // Bind pipeline
    DxvkMetaBlitPipeline pipeInfo = m_common->metaBlit().getPipeline(
      dstView->info().viewType, dstView->info().format,
      dstView->image()->info().sampleCount);

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeHandle);

    // Set up viewport
    VkViewport viewport;
    viewport.x = float(dstOffsetsAdjusted[0].x);
    viewport.y = float(dstOffsetsAdjusted[0].y);
    viewport.width = float(dstExtent.width);
    viewport.height = float(dstExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { dstOffsetsAdjusted[0].x, dstOffsetsAdjusted[0].y  };
    scissor.extent = { dstExtent.width, dstExtent.height };

    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    // Bind source image view
    Rc<DxvkSampler> sampler = createBlitSampler(filter);

    VkDescriptorImageInfo descriptorImage;
    descriptorImage.sampler     = sampler->handle();
    descriptorImage.imageView   = srcView->handle();
    descriptorImage.imageLayout = srcLayout;
    
    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet           = m_descriptorPool->alloc(pipeInfo.dsetLayout);
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo       = &descriptorImage;

    m_cmd->updateDescriptorSets(1, &descriptorWrite);
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeLayout,
      descriptorWrite.dstSet, 0, nullptr);

    // Compute shader parameters for the operation
    VkExtent3D srcExtent = srcView->mipLevelExtent(0);

    DxvkMetaBlitPushConstants pushConstants = { };
    pushConstants.srcCoord0 = {
      float(srcOffsetsAdjusted[0].x) / float(srcExtent.width),
      float(srcOffsetsAdjusted[0].y) / float(srcExtent.height),
      float(srcOffsetsAdjusted[0].z) / float(srcExtent.depth) };
    pushConstants.srcCoord1 = {
      float(srcOffsetsAdjusted[1].x) / float(srcExtent.width),
      float(srcOffsetsAdjusted[1].y) / float(srcExtent.height),
      float(srcOffsetsAdjusted[1].z) / float(srcExtent.depth) };
    pushConstants.layerCount = dstView->info().layerCount;

    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(pushConstants), &pushConstants);

    m_cmd->cmdDraw(3, pushConstants.layerCount, 0, 0);
    m_cmd->cmdEndRendering();

    // Add barriers and track image objects
    m_execBarriers.accessImage(
      dstView->image(),
      dstView->imageSubresources(), dstLayout,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      dstView->image()->info().layout,
      dstView->image()->info().stages,
      dstView->image()->info().access);
    
    m_execBarriers.accessImage(
      srcView->image(),
      srcView->imageSubresources(), srcLayout,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      srcView->image()->info().layout,
      srcView->image()->info().stages,
      srcView->image()->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstView->image());
    m_cmd->trackResource<DxvkAccess::Read>(srcView->image());
    m_cmd->trackSampler(sampler);
  }


  void DxvkContext::blitImageHw(
    const Rc<DxvkImageView>&    dstView,
    const VkOffset3D*           dstOffsets,
    const Rc<DxvkImageView>&    srcView,
    const VkOffset3D*           srcOffsets,
          VkFilter              filter) {
    if (m_execBarriers.isImageDirty(dstView->image(), dstView->imageSubresources(), DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcView->image(), srcView->imageSubresources(), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Prepare the two images for transfer ops if necessary
    auto dstLayout = dstView->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    auto srcLayout = srcView->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    if (dstView->image()->info().layout != dstLayout) {
      m_execAcquires.accessImage(
        dstView->image(),
        dstView->imageSubresources(),
        dstView->image()->info().layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        dstLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    if (srcView->image()->info().layout != srcLayout) {
      m_execAcquires.accessImage(
        srcView->image(),
        srcView->imageSubresources(),
        srcView->image()->info().layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        srcLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);

    // Perform the blit operation
    VkImageBlit2 blitRegion = { VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
    blitRegion.srcSubresource = vk::pickSubresourceLayers(srcView->imageSubresources(), 0);
    blitRegion.dstSubresource = vk::pickSubresourceLayers(dstView->imageSubresources(), 0);

    for (uint32_t i = 0; i < 2; i++) {
      blitRegion.srcOffsets[i] = srcOffsets[i];
      blitRegion.dstOffsets[i] = dstOffsets[i];
    }

    VkBlitImageInfo2 blitInfo = { VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
    blitInfo.srcImage = srcView->image()->handle();
    blitInfo.srcImageLayout = srcLayout;
    blitInfo.dstImage = dstView->image()->handle();
    blitInfo.dstImageLayout = dstLayout;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;
    blitInfo.filter = filter;

    m_cmd->cmdBlitImage(&blitInfo);
    
    m_execBarriers.accessImage(
      dstView->image(),
      dstView->imageSubresources(), dstLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstView->image()->info().layout,
      dstView->image()->info().stages,
      dstView->image()->info().access);
    
    m_execBarriers.accessImage(
      srcView->image(),
      srcView->imageSubresources(), srcLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcView->image()->info().layout,
      srcView->image()->info().stages,
      srcView->image()->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstView->image());
    m_cmd->trackResource<DxvkAccess::Read>(srcView->image());
  }


  template<bool ToImage>
  void DxvkContext::copyImageBufferData(
          DxvkCmdBuffer         cmd,
    const Rc<DxvkImage>&        image,
    const VkImageSubresourceLayers& imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent,
          VkImageLayout         imageLayout,
    const DxvkBufferSliceHandle& bufferSlice,
          VkDeviceSize          bufferRowAlignment,
          VkDeviceSize          bufferSliceAlignment) {
    auto formatInfo = image->formatInfo();
    auto layers = imageSubresource.layerCount;

    VkDeviceSize bufferOffset = bufferSlice.offset;

    // Do one copy region per layer in case the buffer memory layout is weird
    if (bufferSliceAlignment || formatInfo->flags.test(DxvkFormatFlag::MultiPlane))
      layers = 1;

    for (uint32_t i = 0; i < imageSubresource.layerCount; i += layers) {
      auto aspectOffset = bufferOffset;

      for (auto aspects = imageSubresource.aspectMask; aspects; ) {
        auto aspect = vk::getNextAspect(aspects);
        auto elementSize = formatInfo->elementSize;

        VkBufferImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
        copyRegion.imageSubresource.aspectMask = aspect;
        copyRegion.imageSubresource.baseArrayLayer = imageSubresource.baseArrayLayer + i;
        copyRegion.imageSubresource.layerCount = layers;
        copyRegion.imageSubresource.mipLevel = imageSubresource.mipLevel;
        copyRegion.imageOffset = imageOffset;
        copyRegion.imageExtent = imageExtent;

        if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
          auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
          copyRegion.imageOffset.x /= plane->blockSize.width;
          copyRegion.imageOffset.y /= plane->blockSize.height;
          copyRegion.imageExtent.width  /= plane->blockSize.width;
          copyRegion.imageExtent.height /= plane->blockSize.height;
          elementSize = plane->elementSize;
        }

        // Vulkan can't really express row pitch in the same way that client APIs
        // may expect, so we'll need to do some heroics here and hope that it works
        VkExtent3D blockCount = util::computeBlockCount(copyRegion.imageExtent, formatInfo->blockSize);
        VkDeviceSize rowPitch = blockCount.width * elementSize;

        if (bufferRowAlignment > elementSize)
          rowPitch = bufferRowAlignment >= rowPitch ? bufferRowAlignment : align(rowPitch, bufferRowAlignment);

        VkDeviceSize slicePitch = blockCount.height * rowPitch;

        if (image->info().type == VK_IMAGE_TYPE_3D && bufferSliceAlignment > elementSize)
          slicePitch = bufferSliceAlignment >= slicePitch ? bufferSliceAlignment : align(slicePitch, bufferSliceAlignment);

        copyRegion.bufferOffset      = aspectOffset;
        copyRegion.bufferRowLength   = formatInfo->blockSize.width * rowPitch / elementSize;
        copyRegion.bufferImageHeight = formatInfo->blockSize.height * slicePitch / rowPitch;

        // Perform the actual copy
        if constexpr (ToImage) {
          VkCopyBufferToImageInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
          copyInfo.srcBuffer = bufferSlice.handle;
          copyInfo.dstImage = image->handle();
          copyInfo.dstImageLayout = imageLayout;
          copyInfo.regionCount = 1;
          copyInfo.pRegions = &copyRegion;

          m_cmd->cmdCopyBufferToImage(cmd, &copyInfo);
        } else {
          VkCopyImageToBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2 };
          copyInfo.srcImage = image->handle();
          copyInfo.srcImageLayout = imageLayout;
          copyInfo.dstBuffer = bufferSlice.handle;
          copyInfo.regionCount = 1;
          copyInfo.pRegions = &copyRegion;

          m_cmd->cmdCopyImageToBuffer(cmd, &copyInfo);
        }

        aspectOffset += blockCount.depth * slicePitch;
      }

      // Advance to next layer. This is non-trivial for multi-plane formats
      // since plane data for each layer is expected to be packed.
      VkDeviceSize layerPitch = aspectOffset - bufferOffset;

      if (bufferSliceAlignment)
        layerPitch = bufferSliceAlignment >= layerPitch ? bufferSliceAlignment : align(layerPitch, bufferSliceAlignment);

      bufferOffset += layerPitch;
    }
  }


  void DxvkContext::copyBufferToImageHw(
    const Rc<DxvkImage>&        image,
    const VkImageSubresourceLayers& imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          bufferOffset,
          VkDeviceSize          bufferRowAlignment,
          VkDeviceSize          bufferSliceAlignment) {
    this->spillRenderPass(true);
    this->prepareImage(image, vk::makeSubresourceRange(imageSubresource));

    VkDeviceSize dataSize = imageSubresource.layerCount * util::computeImageDataSize(
      image->info().format, imageExtent, imageSubresource.aspectMask);

    auto bufferSlice = buffer->getSliceHandle(bufferOffset, dataSize);

    // We may copy to only one aspect at a time, but pipeline
    // barriers need to have all available aspect bits set
    auto dstFormatInfo = image->formatInfo();

    auto dstSubresourceRange = vk::makeSubresourceRange(imageSubresource);
    dstSubresourceRange.aspectMask = dstFormatInfo->aspectMask;

    if (m_execBarriers.isImageDirty(image, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);

    // Initialize the image if the entire subresource is covered
    VkImageLayout dstImageLayoutInitial = image->info().layout;
    VkImageLayout dstImageLayoutTransfer = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    if (image->isFullSubresource(imageSubresource, imageExtent))
      dstImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;

    m_execAcquires.accessImage(
      image, dstSubresourceRange,
      dstImageLayoutInitial,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
      dstImageLayoutTransfer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);

    m_execAcquires.recordCommands(m_cmd);

    this->copyImageBufferData<true>(DxvkCmdBuffer::ExecBuffer,
      image, imageSubresource, imageOffset, imageExtent, dstImageLayoutTransfer,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);

    m_execBarriers.accessImage(
      image, dstSubresourceRange,
      dstImageLayoutTransfer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);

    m_execBarriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(image);
    m_cmd->trackResource<DxvkAccess::Read>(buffer);
  }


  void DxvkContext::copyBufferToImageFb(
    const Rc<DxvkImage>&        image,
    const VkImageSubresourceLayers& imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          bufferOffset,
          VkDeviceSize          bufferRowAlignment,
          VkDeviceSize          bufferSliceAlignment,
          VkFormat              bufferFormat) {
    this->spillRenderPass(true);
    this->invalidateState();

    this->prepareImage(image, vk::makeSubresourceRange(imageSubresource));

    if (m_execBarriers.isImageDirty(image, vk::makeSubresourceRange(imageSubresource), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    auto formatInfo = lookupFormatInfo(bufferFormat);

    if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
      Logger::err(str::format("DxvkContext: Planar formats not supported for shader-based buffer to image copies"));
      return;
    }

    VkDeviceSize rowPitch = imageExtent.width * formatInfo->elementSize;

    if (bufferRowAlignment > formatInfo->elementSize)
      rowPitch = bufferRowAlignment >= rowPitch ? bufferRowAlignment : align(rowPitch, bufferRowAlignment);

    VkDeviceSize slicePitch = imageExtent.height * rowPitch;

    if (bufferSliceAlignment > formatInfo->elementSize)
      slicePitch = bufferSliceAlignment >= slicePitch ? bufferSliceAlignment : align(slicePitch, bufferSliceAlignment);

    if ((rowPitch % formatInfo->elementSize) || (slicePitch % formatInfo->elementSize)) {
      Logger::err(str::format("DxvkContext: Pitches ", rowPitch, ",", slicePitch, " not a multiple of element size ", formatInfo->elementSize, " for format ", bufferFormat));
      return;
    }

    // Create texel buffer view to read from
    DxvkBufferViewKey bufferViewInfo = { };
    bufferViewInfo.format = sanitizeTexelBufferFormat(bufferFormat);
    bufferViewInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    bufferViewInfo.offset = bufferOffset;
    bufferViewInfo.size = slicePitch * imageExtent.depth * imageSubresource.layerCount;

    Rc<DxvkBufferView> bufferView = buffer->createView(bufferViewInfo);
    VkBufferView bufferViewHandle = bufferView->handle();

    // Create image view to render to
    bool discard = image->isFullSubresource(imageSubresource, imageExtent);
    bool isDepthStencil = imageSubresource.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    DxvkImageViewKey imageViewInfo = { };
    imageViewInfo.viewType = image->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
      : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageViewInfo.format = image->info().format;
    imageViewInfo.usage = isDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageViewInfo.aspects = image->formatInfo()->aspectMask;
    imageViewInfo.mipIndex = imageSubresource.mipLevel;
    imageViewInfo.mipCount = 1u;
    imageViewInfo.layerIndex = imageSubresource.baseArrayLayer;
    imageViewInfo.layerCount = imageSubresource.layerCount;

    if (image->info().type == VK_IMAGE_TYPE_3D) {
      imageViewInfo.layerIndex = imageOffset.z;
      imageViewInfo.layerCount = imageExtent.depth;
    }

    Rc<DxvkImageView> imageView = image->createView(imageViewInfo);

    // Transition image to required layout and discard if possible
    VkImageLayout imageLayout = isDepthStencil
      ? image->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      : image->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkPipelineStageFlags stages = isDepthStencil
      ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
      : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkAccessFlags access = isDepthStencil
      ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
      : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    m_execAcquires.accessImage(image, vk::makeSubresourceRange(imageSubresource),
      discard ? VK_IMAGE_LAYOUT_UNDEFINED : image->info().layout,
      image->info().stages, image->info().access,
      imageLayout, stages, access);

    m_execAcquires.recordCommands(m_cmd);

    // Bind image for rendering
    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = imageView->handle();
    attachment.imageLayout = imageLayout;
    attachment.loadOp = discard
      ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
      : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkExtent3D mipExtent = imageView->mipLevelExtent(0u);

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.extent = { mipExtent.width, mipExtent.height };
    renderingInfo.layerCount = imageViewInfo.layerCount;

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &attachment;
    }

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      renderingInfo.pDepthAttachment = &attachment;

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      renderingInfo.pStencilAttachment = &attachment;

    DxvkBufferSliceHandle bufferSlice = buffer->getSliceHandle(
      bufferOffset, slicePitch * renderingInfo.layerCount);

    if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);

    m_cmd->cmdBeginRendering(&renderingInfo);

    // Set up viewport and scissor state
    VkViewport viewport = { };
    viewport.x = imageOffset.x;
    viewport.y = imageOffset.y;
    viewport.width = imageExtent.width;
    viewport.height = imageExtent.height;
    viewport.maxDepth = 1.0f;

    m_cmd->cmdSetViewport(1, &viewport);

    VkRect2D scissor = { };
    scissor.offset = { imageOffset.x, imageOffset.y };
    scissor.extent = { imageExtent.width, imageExtent.height };

    m_cmd->cmdSetScissor(1, &scissor);

    // Get pipeline and descriptor set layout. All pipelines
    // will be using the same pipeline layout here.
    bool needsBitwiseStencilCopy = !m_device->features().extShaderStencilExport
      && (imageSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT);

    // If we have a depth aspect, this will give us either the depth-only
    // pipeline or one that can write all the given aspects
    DxvkMetaCopyPipeline pipeline = m_common->metaCopy().getCopyBufferToImagePipeline(
      image->info().format, bufferFormat, imageSubresource.aspectMask);

    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet = m_descriptorPool->alloc(pipeline.dsetLayout);
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pTexelBufferView = &bufferViewHandle;

    m_cmd->updateDescriptorSets(1, &descriptorWrite);

    DxvkBufferImageCopyArgs pushConst = { };
    pushConst.imageOffset = imageOffset;
    pushConst.bufferOffset = 0u;
    pushConst.imageExtent = imageExtent;
    pushConst.bufferImageWidth = rowPitch / formatInfo->elementSize;
    pushConst.bufferImageHeight = slicePitch / rowPitch;

    if (imageSubresource.aspectMask != VK_IMAGE_ASPECT_STENCIL_BIT || !needsBitwiseStencilCopy) {
      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeHandle);

      m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeLayout,
        descriptorWrite.dstSet, 0, nullptr);

      m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
        pipeline.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(pushConst), &pushConst);

      m_cmd->cmdDraw(3, renderingInfo.layerCount, 0, 0);
    }

    if (needsBitwiseStencilCopy) {
      // On systems that do not support stencil export, we need to clear
      // stencil to 0 and then "write" each individual bit by discarding
      // fragments where that bit is not set.
      pipeline = m_common->metaCopy().getCopyBufferToImagePipeline(
        image->info().format, bufferFormat, VK_IMAGE_ASPECT_STENCIL_BIT);

      if (imageSubresource.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) {
        VkClearAttachment clear = { };
        clear.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

        VkClearRect clearRect = { };
        clearRect.rect = scissor;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = renderingInfo.layerCount;

        m_cmd->cmdClearAttachments(1, &clear, 1, &clearRect);
      }

      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeHandle);

      m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeLayout,
        descriptorWrite.dstSet, 0, nullptr);

      for (uint32_t i = 0; i < 8; i++) {
        pushConst.stencilBitIndex = i;

        m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
          pipeline.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
          0, sizeof(pushConst), &pushConst);

        m_cmd->cmdSetStencilWriteMask(VK_STENCIL_FACE_FRONT_AND_BACK, 1u << i);
        m_cmd->cmdDraw(3, renderingInfo.layerCount, 0, 0);
      }
    }

    m_cmd->cmdEndRendering();

    m_execBarriers.accessImage(image,
      vk::makeSubresourceRange(imageSubresource),
      imageLayout, stages, access,
      image->info().layout, image->info().stages, image->info().access);

    m_execBarriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(image);
    m_cmd->trackResource<DxvkAccess::Read>(buffer);
  }


  void DxvkContext::copyImageToBufferHw(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          bufferOffset,
          VkDeviceSize          bufferRowAlignment,
          VkDeviceSize          bufferSliceAlignment,
    const Rc<DxvkImage>&        image,
          VkImageSubresourceLayers imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent) {
    this->spillRenderPass(true);
    this->prepareImage(image, vk::makeSubresourceRange(imageSubresource));

    VkDeviceSize dataSize = imageSubresource.layerCount * util::computeImageDataSize(
      image->info().format, imageExtent, imageSubresource.aspectMask);

    auto bufferSlice = buffer->getSliceHandle(bufferOffset, dataSize);

    // We may copy to only one aspect of a depth-stencil image,
    // but pipeline barriers need to have all aspect bits set
    auto srcFormatInfo = image->formatInfo();

    auto srcSubresourceRange = vk::makeSubresourceRange(imageSubresource);
    srcSubresourceRange.aspectMask = srcFormatInfo->aspectMask;

    if (m_execBarriers.isImageDirty(image, srcSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Select a suitable image layout for the transfer op
    VkImageLayout srcImageLayoutTransfer = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    m_execAcquires.accessImage(
      image, srcSubresourceRange,
      image->info().layout,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
      srcImageLayoutTransfer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);

    m_execAcquires.recordCommands(m_cmd);

    this->copyImageBufferData<false>(DxvkCmdBuffer::ExecBuffer,
      image, imageSubresource, imageOffset, imageExtent, srcImageLayoutTransfer,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);

    m_execBarriers.accessImage(
      image, srcSubresourceRange,
      srcImageLayoutTransfer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);

    m_execBarriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(buffer);
    m_cmd->trackResource<DxvkAccess::Read>(image);
  }


  void DxvkContext::copyImageToBufferFb(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          bufferOffset,
          VkDeviceSize          bufferRowAlignment,
          VkDeviceSize          bufferSliceAlignment,
          VkFormat              bufferFormat,
    const Rc<DxvkImage>&        image,
          VkImageSubresourceLayers imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent) {
    this->spillRenderPass(true);
    this->invalidateState();

    this->prepareImage(image, vk::makeSubresourceRange(imageSubresource));

    // Ensure we can read the source image
    DxvkImageUsageInfo imageUsage = { };
    imageUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    ensureImageCompatibility(image, imageUsage);

    if (m_execBarriers.isImageDirty(image, vk::makeSubresourceRange(imageSubresource), DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    auto formatInfo = lookupFormatInfo(bufferFormat);

    if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
      Logger::err(str::format("DxvkContext: Planar formats not supported for shader-based image to buffer copies"));
      return;
    }

    VkDeviceSize rowPitch = imageExtent.width * formatInfo->elementSize;

    if (bufferRowAlignment > formatInfo->elementSize)
      rowPitch = bufferRowAlignment >= rowPitch ? bufferRowAlignment : align(rowPitch, bufferRowAlignment);

    VkDeviceSize slicePitch = imageExtent.height * rowPitch;

    if (bufferSliceAlignment > formatInfo->elementSize)
      slicePitch = bufferSliceAlignment >= slicePitch ? bufferSliceAlignment : align(slicePitch, bufferSliceAlignment);

    if ((rowPitch % formatInfo->elementSize) || (slicePitch % formatInfo->elementSize)) {
      Logger::err(str::format("DxvkContext: Pitches ", rowPitch, ",", slicePitch, " not a multiple of element size ", formatInfo->elementSize, " for format ", bufferFormat));
      return;
    }

    // Create texel buffer view to write to
    DxvkBufferViewKey bufferViewInfo = { };
    bufferViewInfo.format = sanitizeTexelBufferFormat(bufferFormat);
    bufferViewInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    bufferViewInfo.offset = bufferOffset;
    bufferViewInfo.size = slicePitch * imageExtent.depth * imageSubresource.layerCount;

    Rc<DxvkBufferView> bufferView = buffer->createView(bufferViewInfo);
    VkBufferView bufferViewHandle = bufferView->handle();

    DxvkBufferSliceHandle bufferSlice = buffer->getSliceHandle(
      bufferViewInfo.offset, bufferViewInfo.size);

    if (m_execBarriers.isBufferDirty(bufferSlice, DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);

    // Transition image to a layout we can use for reading as necessary
    VkImageLayout imageLayout = (image->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? image->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      : image->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (imageLayout != image->info().layout) {
      m_execAcquires.accessImage(image,
        vk::makeSubresourceRange(imageSubresource),
        image->info().layout,
        image->info().stages,
        image->info().access,
        imageLayout,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);

      m_execAcquires.recordCommands(m_cmd);
    }

    // Retrieve pipeline
    VkImageViewType viewType = image->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
      : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    if (image->info().type == VK_IMAGE_TYPE_3D)
      viewType = VK_IMAGE_VIEW_TYPE_3D;

    DxvkMetaCopyPipeline pipeline = m_common->metaCopy().getCopyImageToBufferPipeline(viewType, bufferFormat);

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeHandle);

    // Create image views  for the main and stencil aspects
    VkDescriptorImageInfo imageDescriptor = { };
    VkDescriptorImageInfo stencilDescriptor = { };

    Rc<DxvkImageView> imageView;
    Rc<DxvkImageView> stencilView;

    DxvkImageViewKey imageViewInfo;
    imageViewInfo.viewType = viewType;
    imageViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageViewInfo.format = image->info().format;
    imageViewInfo.aspects = imageSubresource.aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);
    imageViewInfo.mipIndex = imageSubresource.mipLevel;
    imageViewInfo.mipCount = 1;
    imageViewInfo.layerIndex = imageSubresource.baseArrayLayer;
    imageViewInfo.layerCount = imageSubresource.layerCount;

    if (imageViewInfo.aspects) {
      imageView = image->createView(imageViewInfo);

      imageDescriptor.imageView = imageView->handle();
      imageDescriptor.imageLayout = imageLayout;
    }

    imageViewInfo.aspects = imageSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT;

    if (imageViewInfo.aspects) {
      stencilView = image->createView(imageViewInfo);

      stencilDescriptor.imageView = stencilView->handle();
      stencilDescriptor.imageLayout = imageLayout;
    }

    VkDescriptorSet set = m_descriptorPool->alloc(pipeline.dsetLayout);

    std::array<VkWriteDescriptorSet, 3> descriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, nullptr, nullptr, &bufferViewHandle },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 1, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &stencilDescriptor },
    }};

    m_cmd->updateDescriptorSets(
      descriptorWrites.size(),
      descriptorWrites.data());

    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeLayout,
      set, 0, nullptr);

    // Set up shader arguments
    DxvkBufferImageCopyArgs pushConst = { };
    pushConst.imageOffset = imageOffset;
    pushConst.bufferOffset = 0u;
    pushConst.imageExtent = imageExtent;
    pushConst.bufferImageWidth = rowPitch / formatInfo->elementSize;
    pushConst.bufferImageHeight = slicePitch / rowPitch;

    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeline.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushConst), &pushConst);

    // Compute number of workgroups and dispatch shader
    VkExtent3D workgroupCount = imageExtent;
    workgroupCount.depth *= imageSubresource.layerCount;
    workgroupCount = util::computeBlockCount(workgroupCount, { 16, 16, 1 });

    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      workgroupCount.width,
      workgroupCount.height,
      workgroupCount.depth);

    m_execBarriers.accessBuffer(bufferSlice,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_execBarriers.accessImage(image,
      vk::makeSubresourceRange(imageSubresource),
      imageLayout,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      image->info().layout,
      image->info().stages,
      image->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(buffer);
    m_cmd->trackResource<DxvkAccess::Read>(image);
  }


  void DxvkContext::clearImageViewFb(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkImageAspectFlags    aspect,
          VkClearValue          value) {
    this->updateFramebuffer();

    VkPipelineStageFlags clearStages = 0;
    VkAccessFlags clearAccess = 0;
    VkImageLayout clearLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Find out if the render target view is currently bound,
    // so that we can avoid spilling the render pass if it is.
    int32_t attachmentIndex = -1;
    
    if (m_state.om.framebufferInfo.isFullSize(imageView))
      attachmentIndex = m_state.om.framebufferInfo.findAttachment(imageView);

    if (attachmentIndex >= 0 && !m_state.om.framebufferInfo.isWritable(attachmentIndex, aspect))
      attachmentIndex = -1;

    if (attachmentIndex < 0) {
      this->spillRenderPass(false);

      if (m_execBarriers.isImageDirty(imageView->image(), imageView->imageSubresources(), DxvkAccess::Write))
        m_execBarriers.recordCommands(m_cmd);

      clearLayout = (imageView->info().aspects & VK_IMAGE_ASPECT_COLOR_BIT)
        ? imageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        : imageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

      VkExtent3D extent = imageView->mipLevelExtent(0);

      VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
      attachmentInfo.imageView = imageView->handle();
      attachmentInfo.imageLayout = clearLayout;
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
      renderingInfo.renderArea.extent = { extent.width, extent.height };
      renderingInfo.layerCount = imageView->info().layerCount;

      if (imageView->info().aspects & VK_IMAGE_ASPECT_COLOR_BIT) {
        clearStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        clearAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                    |  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;
      } else {
        clearStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        clearAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                    |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        if (imageView->info().aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
          renderingInfo.pDepthAttachment = &attachmentInfo;

        if (imageView->info().aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
          renderingInfo.pStencilAttachment = &attachmentInfo;
      }

      if (clearLayout != imageView->image()->info().layout) {
        m_execAcquires.accessImage(
          imageView->image(),
          imageView->imageSubresources(),
          imageView->image()->info().layout, clearStages, 0,
          clearLayout, clearStages, clearAccess);
        m_execAcquires.recordCommands(m_cmd);
      }

      // We cannot leverage render pass clears
      // because we clear only part of the view
      m_cmd->cmdBeginRendering(&renderingInfo);
    } else {
      // Make sure the render pass is active so
      // that we can actually perform the clear
      this->startRenderPass();
    }

    // Perform the actual clear operation
    VkClearAttachment clearInfo;
    clearInfo.aspectMask          = aspect;
    clearInfo.colorAttachment     = 0;
    clearInfo.clearValue          = value;

    if ((aspect & VK_IMAGE_ASPECT_COLOR_BIT) && (attachmentIndex >= 0))
      clearInfo.colorAttachment   = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

    VkClearRect clearRect;
    clearRect.rect.offset.x       = offset.x;
    clearRect.rect.offset.y       = offset.y;
    clearRect.rect.extent.width   = extent.width;
    clearRect.rect.extent.height  = extent.height;
    clearRect.baseArrayLayer      = 0;
    clearRect.layerCount          = imageView->info().layerCount;

    m_cmd->cmdClearAttachments(1, &clearInfo, 1, &clearRect);

    // Unbind temporary framebuffer
    if (attachmentIndex < 0) {
      m_cmd->cmdEndRendering();

      m_execBarriers.accessImage(
        imageView->image(),
        imageView->imageSubresources(),
        clearLayout, clearStages, clearAccess,
        imageView->image()->info().layout,
        imageView->image()->info().stages,
        imageView->image()->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(imageView->image());
    }
  }

  
  void DxvkContext::clearImageViewCs(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkClearValue          value) {
    this->spillRenderPass(false);
    this->invalidateState();
    
    if (m_execBarriers.isImageDirty(
          imageView->image(),
          imageView->imageSubresources(),
          DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);
    
    // Query pipeline objects to use for this clear operation
    DxvkMetaClearPipeline pipeInfo = m_common->metaClear().getClearImagePipeline(
      imageView->type(), lookupFormatInfo(imageView->info().format)->flags);
    
    // Create a descriptor set pointing to the view
    VkDescriptorSet descriptorSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);
    
    VkDescriptorImageInfo viewInfo;
    viewInfo.sampler      = VK_NULL_HANDLE;
    viewInfo.imageView    = imageView->handle();
    viewInfo.imageLayout  = imageView->image()->info().layout;
    
    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.pImageInfo       = &viewInfo;
    m_cmd->updateDescriptorSets(1, &descriptorWrite);
    
    // Prepare shader arguments
    DxvkMetaClearArgs pushArgs = { };
    pushArgs.clearValue = value.color;
    pushArgs.offset = offset;
    pushArgs.extent = extent;
    
    VkExtent3D workgroups = util::computeBlockCount(
      pushArgs.extent, pipeInfo.workgroupSize);
    
    if (imageView->type() == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
      workgroups.height = imageView->subresources().layerCount;
    else if (imageView->type() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
      workgroups.depth = imageView->subresources().layerCount;
    
    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      workgroups.width, workgroups.height, workgroups.depth);
    
    m_execBarriers.accessImage(
      imageView->image(),
      imageView->imageSubresources(),
      imageView->image()->info().layout,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      imageView->image()->info().layout,
      imageView->image()->info().stages,
      imageView->image()->info().access);
    
    m_cmd->trackResource<DxvkAccess::Write>(imageView->image());
  }

  
  void DxvkContext::copyImageHw(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    auto dstSubresourceRange = vk::makeSubresourceRange(dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(srcSubresource);

    auto dstFormatInfo = dstImage->formatInfo();

    if (m_execBarriers.isImageDirty(dstImage, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcImage, srcSubresourceRange, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    VkImageLayout dstImageLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageLayout srcImageLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageLayout dstInitImageLayout = dstImage->info().layout;

    if (dstImage->isFullSubresource(dstSubresource, extent))
      dstInitImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (dstImageLayout != dstInitImageLayout) {
      m_execAcquires.accessImage(
        dstImage, dstSubresourceRange,
        dstInitImageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        dstImageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    if (srcImageLayout != srcImage->info().layout) {
      m_execAcquires.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->info().layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        srcImageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);
    
    for (auto aspects = dstSubresource.aspectMask; aspects; ) {
      auto aspect = vk::getNextAspect(aspects);

      VkImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
      copyRegion.srcSubresource = srcSubresource;
      copyRegion.srcSubresource.aspectMask = aspect;
      copyRegion.srcOffset = srcOffset;
      copyRegion.dstSubresource = dstSubresource;
      copyRegion.dstSubresource.aspectMask = aspect;
      copyRegion.dstOffset = dstOffset;
      copyRegion.extent = extent;

      if (dstFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
        auto plane = &dstFormatInfo->planes[vk::getPlaneIndex(aspect)];
        copyRegion.srcOffset.x /= plane->blockSize.width;
        copyRegion.srcOffset.y /= plane->blockSize.height;
        copyRegion.dstOffset.x /= plane->blockSize.width;
        copyRegion.dstOffset.y /= plane->blockSize.height;
        copyRegion.extent.width /= plane->blockSize.width;
        copyRegion.extent.height /= plane->blockSize.height;
      }

      VkCopyImageInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
      copyInfo.srcImage = srcImage->handle();
      copyInfo.srcImageLayout = srcImageLayout;
      copyInfo.dstImage = dstImage->handle();
      copyInfo.dstImageLayout = dstImageLayout;
      copyInfo.regionCount = 1;
      copyInfo.pRegions = &copyRegion;

      m_cmd->cmdCopyImage(DxvkCmdBuffer::ExecBuffer, &copyInfo);
    }
    
    m_execBarriers.accessImage(
      dstImage, dstSubresourceRange,
      dstImageLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);

    m_execBarriers.accessImage(
      srcImage, srcSubresourceRange,
      srcImageLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);
    
    m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    m_cmd->trackResource<DxvkAccess::Read>(srcImage);
  }

  
  void DxvkContext::copyImageFb(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    DxvkMetaCopyFormats viewFormats = m_common->metaCopy().getCopyImageFormats(
      dstImage->info().format, dstSubresource.aspectMask,
      srcImage->info().format, srcSubresource.aspectMask);
    
    // Guarantee that we can render to or sample the images
    DxvkImageUsageInfo dstUsage = { };
    dstUsage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    dstUsage.viewFormatCount = 1;
    dstUsage.viewFormats = &viewFormats.dstFormat;

    if (dstImage->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      dstUsage.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    DxvkImageUsageInfo srcUsage = { };
    srcUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    srcUsage.viewFormatCount = 1;
    srcUsage.viewFormats = &viewFormats.srcFormat;

    if (!ensureImageCompatibility(dstImage, dstUsage)
     || !ensureImageCompatibility(srcImage, srcUsage)) {
      Logger::err(str::format("DxvkContext: copyImageFb: Unsupported images:"
        "\n  dst format: ", dstImage->info().format,
        "\n  src format: ", srcImage->info().format));
      return;
    }

    this->invalidateState();

    auto dstSubresourceRange = vk::makeSubresourceRange(dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(srcSubresource);

    if (m_execBarriers.isImageDirty(dstImage, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcImage, srcSubresourceRange, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Flag used to determine whether we can do an UNDEFINED transition    
    bool doDiscard = dstImage->isFullSubresource(dstSubresource, extent);

    // This function can process both color and depth-stencil images, so
    // some things change a lot depending on the destination image type
    VkPipelineStageFlags dstStages;
    VkAccessFlags dstAccess;
    VkImageLayout dstLayout;

    if (dstSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
      dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      dstStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    // Might have to transition source image as well
    VkImageLayout srcLayout = (srcSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      ? srcImage->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      : srcImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    
    if (srcImage->info().layout != srcLayout) {
      m_execAcquires.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->info().layout,
        srcImage->info().stages, 0,
        srcLayout,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    }

    if (dstImage->info().layout != dstLayout || doDiscard) {
      m_execAcquires.accessImage(
        dstImage, dstSubresourceRange,
        doDiscard ? VK_IMAGE_LAYOUT_UNDEFINED
                  : dstImage->info().layout,
        dstImage->info().stages, 0,
        dstLayout, dstStages, dstAccess);
    }

    m_execAcquires.recordCommands(m_cmd);

    // Create source and destination image views
    DxvkMetaCopyViews views(
      dstImage, dstSubresource, viewFormats.dstFormat,
      srcImage, srcSubresource, viewFormats.srcFormat);

    // Create pipeline for the copy operation
    DxvkMetaCopyPipeline pipeInfo = m_common->metaCopy().getCopyImagePipeline(
      views.srcImageView->info().viewType, viewFormats.dstFormat, dstImage->info().sampleCount);

    // Create and initialize descriptor set    
    VkDescriptorSet descriptorSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);

    std::array<VkDescriptorImageInfo, 2> descriptorImages = {{
      { VK_NULL_HANDLE, views.srcImageView->handle(), srcLayout },
      { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },
    }};

    if (views.srcStencilView) {
      descriptorImages[1].imageView = views.srcStencilView->handle();
      descriptorImages[1].imageLayout = srcLayout;
    }

    std::array<VkWriteDescriptorSet, 2> descriptorWrites;

    for (uint32_t i = 0; i < descriptorWrites.size(); i++) {
      descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      descriptorWrites[i].dstSet = descriptorSet;
      descriptorWrites[i].dstBinding = i;
      descriptorWrites[i].descriptorCount = 1;
      descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      descriptorWrites[i].pImageInfo = &descriptorImages[i];
    }

    m_cmd->updateDescriptorSets(
      descriptorWrites.size(),
      descriptorWrites.data());

    // Set up render state    
    VkViewport viewport;
    viewport.x = float(dstOffset.x);
    viewport.y = float(dstOffset.y);
    viewport.width = float(extent.width);
    viewport.height = float(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { dstOffset.x, dstOffset.y };
    scissor.extent = { extent.width, extent.height };

    VkExtent3D mipExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = views.dstImageView->handle();
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    if (doDiscard)
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { mipExtent.width, mipExtent.height };
    renderingInfo.layerCount = dstSubresource.layerCount;

    VkImageAspectFlags dstAspects = dstImage->formatInfo()->aspectMask;

    if (dstAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &attachmentInfo;
    } else {
      if (dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        renderingInfo.pDepthAttachment = &attachmentInfo;
      if (dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        renderingInfo.pStencilAttachment = &attachmentInfo;
    }

    // Perform the actual copy operation
    m_cmd->cmdBeginRendering(&renderingInfo);
    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeHandle);
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);

    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    VkOffset2D srcCoordOffset = {
      srcOffset.x - dstOffset.x,
      srcOffset.y - dstOffset.y };
    
    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(srcCoordOffset), &srcCoordOffset);
    
    m_cmd->cmdDraw(3, dstSubresource.layerCount, 0, 0);
    m_cmd->cmdEndRendering();

    m_execBarriers.accessImage(
      srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);

    m_execBarriers.accessImage(
      dstImage, dstSubresourceRange,
      dstLayout, dstStages, dstAccess,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    m_cmd->trackResource<DxvkAccess::Read>(srcImage);
  }


  bool DxvkContext::copyImageClear(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
          VkExtent3D            dstExtent,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource) {
    // If the source image has a pending deferred clear, we can
    // implement the copy by clearing the destination image to
    // the same clear value.
    const VkImageUsageFlags attachmentUsage
      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (!(dstImage->info().usage & attachmentUsage)
     || !(srcImage->info().usage & attachmentUsage))
      return false;

    // Ignore 3D images since those are complicated to handle
    if (dstImage->info().type == VK_IMAGE_TYPE_3D
     || srcImage->info().type == VK_IMAGE_TYPE_3D)
      return false;

    // Find a pending clear that overlaps with the source image
    const DxvkDeferredClear* clear = nullptr;

    for (const auto& entry : m_deferredClears) {
      // Entries in the deferred clear array cannot overlap, so
      // if we find an entry covering all source subresources,
      // it's the only one in the list that does.
      if ((entry.imageView->image() == srcImage) && ((srcSubresource.aspectMask & entry.clearAspects) == srcSubresource.aspectMask)
       && (vk::checkSubresourceRangeSuperset(entry.imageView->subresources(), vk::makeSubresourceRange(srcSubresource)))) {
        clear = &entry;
        break;
      }
    }

    if (!clear)
      return false;

    // Create a view for the destination image with the general
    // properties ofthe source image view used for the clear
    DxvkImageViewKey viewInfo = clear->imageView->info();
    viewInfo.viewType = dstImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
      : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.mipIndex = dstSubresource.mipLevel;
    viewInfo.mipCount = 1;
    viewInfo.layerIndex = dstSubresource.baseArrayLayer;
    viewInfo.layerCount = dstSubresource.layerCount;

    // That is, if the formats are actually compatible
    // so that we can safely use the same clear value
    if (!dstImage->isViewCompatible(viewInfo.format))
      return false;

    // Ignore mismatched size for now, needs more testing since we'd
    // need to prepare the image first and then call clearImageViewFb
    if (dstImage->mipLevelExtent(dstSubresource.mipLevel) != dstExtent)
      return false;

    auto view = dstImage->createView(viewInfo);
    this->deferClear(view, srcSubresource.aspectMask, clear->clearValue);
    return true;
  }


  template<bool ToBuffer>
  void DxvkContext::copySparsePages(
    const Rc<DxvkPagedResource>& sparse,
          uint32_t              pageCount,
    const uint32_t*             pages,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset) {
    auto pageTable = sparse->getSparsePageTable();
    auto bufferHandle = buffer->getSliceHandle(offset, SparseMemoryPageSize * pageCount);

    if (m_execBarriers.isBufferDirty(bufferHandle,
        ToBuffer ? DxvkAccess::Write : DxvkAccess::Read))
      m_execBarriers.recordCommands(m_cmd);

    if (pageTable->getBufferHandle()) {
      this->copySparseBufferPages<ToBuffer>(
        static_cast<DxvkBuffer*>(sparse.ptr()),
        pageCount, pages, buffer, offset);
    } else {
      this->copySparseImagePages<ToBuffer>(
        static_cast<DxvkImage*>(sparse.ptr()),
        pageCount, pages, buffer, offset);
    }
  }


  template<bool ToBuffer>
  void DxvkContext::copySparseBufferPages(
    const Rc<DxvkBuffer>&       sparse,
          uint32_t              pageCount,
    const uint32_t*             pages,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset) {
    std::vector<VkBufferCopy2> regions;
    regions.reserve(pageCount);

    auto pageTable = sparse->getSparsePageTable();

    auto sparseHandle = sparse->getSliceHandle();
    auto bufferHandle = buffer->getSliceHandle(offset, SparseMemoryPageSize * pageCount);

    if (m_execBarriers.isBufferDirty(sparseHandle,
        ToBuffer ? DxvkAccess::Read : DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    for (uint32_t i = 0; i < pageCount; i++) {
      auto pageInfo = pageTable->getPageInfo(pages[i]);

      if (pageInfo.type == DxvkSparsePageType::Buffer) {
        VkDeviceSize sparseOffset = pageInfo.buffer.offset;
        VkDeviceSize bufferOffset = bufferHandle.offset + SparseMemoryPageSize * i;

        VkBufferCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
        copy.srcOffset = ToBuffer ? sparseOffset : bufferOffset;
        copy.dstOffset = ToBuffer ? bufferOffset : sparseOffset;
        copy.size = pageInfo.buffer.length;

        regions.push_back(copy);
      }
    }

    VkCopyBufferInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    info.srcBuffer = ToBuffer ? sparseHandle.handle : bufferHandle.handle;
    info.dstBuffer = ToBuffer ? bufferHandle.handle : sparseHandle.handle;
    info.regionCount = uint32_t(regions.size());
    info.pRegions = regions.data();

    if (info.regionCount)
      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &info);

    m_execBarriers.accessBuffer(sparseHandle,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT,
      sparse->info().stages,
      sparse->info().access);

    m_execBarriers.accessBuffer(bufferHandle,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<ToBuffer ? DxvkAccess::Read : DxvkAccess::Write>(sparse);
    m_cmd->trackResource<ToBuffer ? DxvkAccess::Write : DxvkAccess::Read>(buffer);
  }


  template<bool ToBuffer>
  void DxvkContext::copySparseImagePages(
    const Rc<DxvkImage>&        sparse,
          uint32_t              pageCount,
    const uint32_t*             pages,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset) {
    std::vector<VkBufferImageCopy2> regions;
    regions.reserve(pageCount);

    auto pageTable = sparse->getSparsePageTable();
    auto pageExtent = pageTable->getProperties().pageRegionExtent;

    auto bufferHandle = buffer->getSliceHandle(offset, SparseMemoryPageSize * pageCount);
    auto sparseSubresources = sparse->getAvailableSubresources();

    if (m_execBarriers.isImageDirty(sparse, sparseSubresources, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    VkImageLayout transferLayout = sparse->pickLayout(ToBuffer
      ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
      : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkAccessFlags transferAccess = ToBuffer
      ? VK_ACCESS_TRANSFER_READ_BIT
      : VK_ACCESS_TRANSFER_WRITE_BIT;

    if (sparse->info().layout != transferLayout) {
      m_execAcquires.accessImage(sparse, sparseSubresources,
        sparse->info().layout,
        sparse->info().stages, 0,
        transferLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        transferAccess);

      m_execAcquires.recordCommands(m_cmd);
    }

    for (uint32_t i = 0; i < pageCount; i++) {
      auto pageInfo = pageTable->getPageInfo(pages[i]);

      if (pageInfo.type == DxvkSparsePageType::Image) {
        VkBufferImageCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
        copy.bufferOffset = bufferHandle.offset + SparseMemoryPageSize * i;
        copy.bufferRowLength = pageExtent.width;
        copy.bufferImageHeight = pageExtent.height;
        copy.imageSubresource = vk::makeSubresourceLayers(pageInfo.image.subresource);
        copy.imageOffset = pageInfo.image.offset;
        copy.imageExtent = pageInfo.image.extent;

        regions.push_back(copy);
      }
    }

    if (ToBuffer) {
      VkCopyImageToBufferInfo2 info = { VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2 };
      info.srcImage = sparse->handle();
      info.srcImageLayout = transferLayout;
      info.dstBuffer = bufferHandle.handle;
      info.regionCount = regions.size();
      info.pRegions = regions.data();

      if (info.regionCount)
        m_cmd->cmdCopyImageToBuffer(DxvkCmdBuffer::ExecBuffer, &info);
    } else {
      VkCopyBufferToImageInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
      info.srcBuffer = bufferHandle.handle;
      info.dstImage = sparse->handle();
      info.dstImageLayout = transferLayout;
      info.regionCount = regions.size();
      info.pRegions = regions.data();

      if (info.regionCount)
        m_cmd->cmdCopyBufferToImage(DxvkCmdBuffer::ExecBuffer, &info);
    }

    m_execBarriers.accessImage(sparse, sparseSubresources,
      transferLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      transferAccess,
      sparse->info().layout,
      sparse->info().stages,
      sparse->info().access);

    m_execBarriers.accessBuffer(bufferHandle,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT,
      buffer->info().stages,
      buffer->info().access);

    m_cmd->trackResource<ToBuffer ? DxvkAccess::Read : DxvkAccess::Write>(sparse);
    m_cmd->trackResource<ToBuffer ? DxvkAccess::Write : DxvkAccess::Read>(buffer);
  }


  void DxvkContext::resolveImageHw(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);
    
    if (m_execBarriers.isImageDirty(dstImage, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcImage, srcSubresourceRange, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);
    
    // We only support resolving to the entire image
    // area, so we might as well discard its contents
    VkImageLayout dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageLayout srcLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageLayout initialLayout = dstImage->info().layout;

    if (dstImage->isFullSubresource(region.dstSubresource, region.extent))
      initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (dstLayout != initialLayout) {
      m_execAcquires.accessImage(
        dstImage, dstSubresourceRange, initialLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        dstLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    if (srcLayout != srcImage->info().layout) {
      m_execAcquires.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->info().layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        srcLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);

    VkImageResolve2 resolveRegion = { VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2 };
    resolveRegion.srcSubresource = region.srcSubresource;
    resolveRegion.srcOffset = region.srcOffset;
    resolveRegion.dstSubresource = region.dstSubresource;
    resolveRegion.dstOffset = region.dstOffset;
    resolveRegion.extent = region.extent;

    VkResolveImageInfo2 resolveInfo = { VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2 };
    resolveInfo.srcImage = srcImage->handle();
    resolveInfo.srcImageLayout = srcLayout;
    resolveInfo.dstImage = dstImage->handle();
    resolveInfo.dstImageLayout = dstLayout;
    resolveInfo.regionCount = 1;
    resolveInfo.pRegions = &resolveRegion;

    m_cmd->cmdResolveImage(&resolveInfo);
  
    m_execBarriers.accessImage(
      dstImage, dstSubresourceRange, dstLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);

    m_execBarriers.accessImage(
      srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);
    
    m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    m_cmd->trackResource<DxvkAccess::Read>(srcImage);
  }


  void DxvkContext::resolveImageDs(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkResolveModeFlagBits     depthMode,
          VkResolveModeFlagBits     stencilMode) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    DxvkImageUsageInfo usageInfo = { };
    usageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (!ensureImageCompatibility(dstImage, usageInfo)
     || !ensureImageCompatibility(srcImage, usageInfo)) {
      Logger::err(str::format("DxvkContext: resolveImageDs: Unsupported images:"
        "\n  dst format: ", dstImage->info().format,
        "\n  src format: ", srcImage->info().format));
    }

    if (m_execBarriers.isImageDirty(dstImage, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcImage, srcSubresourceRange, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Transition both images to usable layouts if necessary. For the source image we
    // can be fairly leniet since writable layouts are allowed for resolve attachments.
    VkImageLayout dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkImageLayout srcLayout = srcImage->info().layout;

    if (srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
     && srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      srcLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    if (srcImage->info().layout != srcLayout) {
      m_execAcquires.accessImage(srcImage, srcSubresourceRange,
        srcImage->info().layout,
        srcImage->info().stages, 0,
        srcLayout,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    }

    if (dstImage->info().layout != dstLayout) {
      m_execAcquires.accessImage(dstImage, dstSubresourceRange,
        VK_IMAGE_LAYOUT_UNDEFINED, dstImage->info().stages, 0,
        dstLayout,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);

    // Create a pair of views for the attachment resolve
    DxvkMetaResolveViews views(dstImage, region.dstSubresource,
      srcImage, region.srcSubresource, dstImage->info().format);

    VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    depthAttachment.imageView = views.srcView->handle();
    depthAttachment.imageLayout = srcLayout;
    depthAttachment.resolveMode = depthMode;
    depthAttachment.resolveImageView = views.dstView->handle();
    depthAttachment.resolveImageLayout = dstLayout;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo stencilAttachment = depthAttachment;
    stencilAttachment.resolveMode = stencilMode;

    VkExtent3D extent = dstImage->mipLevelExtent(region.dstSubresource.mipLevel);

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { extent.width, extent.height };
    renderingInfo.layerCount = region.dstSubresource.layerCount;

    if (dstImage->formatInfo()->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      renderingInfo.pDepthAttachment = &depthAttachment;

    if (dstImage->formatInfo()->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      renderingInfo.pStencilAttachment = &stencilAttachment;

    m_cmd->cmdBeginRendering(&renderingInfo);
    m_cmd->cmdEndRendering();

    // Add barriers for the resolve operation
    m_execBarriers.accessImage(srcImage, srcSubresourceRange,
      srcLayout,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);

    m_execBarriers.accessImage(dstImage, dstSubresourceRange,
      dstLayout,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    m_cmd->trackResource<DxvkAccess::Read>(srcImage);
  }


  void DxvkContext::resolveImageFb(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format,
          VkResolveModeFlagBits     depthMode,
          VkResolveModeFlagBits     stencilMode) {
    this->invalidateState();

    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    // Ensure we can access the destination image for rendering,
    // and the source image for reading within the shader.
    DxvkImageUsageInfo dstUsage = { };
    dstUsage.usage = (dstImage->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    DxvkImageUsageInfo srcUsage = { };
    srcUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (format) {
      dstUsage.viewFormatCount = 1u;
      dstUsage.viewFormats = &format;

      srcUsage.viewFormatCount = 1u;
      srcUsage.viewFormats = &format;
    }

    if (!ensureImageCompatibility(dstImage, dstUsage)
     || !ensureImageCompatibility(srcImage, srcUsage)) {
      Logger::err(str::format("DxvkContext: resolveImageFb: Unsupported images:",
        "\n  dst format:  ", dstImage->info().format,
        "\n  src format:  ", srcImage->info().format,
        "\n  view format: ", format));
      return;
    }

    if (m_execBarriers.isImageDirty(dstImage, dstSubresourceRange, DxvkAccess::Write)
     || m_execBarriers.isImageDirty(srcImage, srcSubresourceRange, DxvkAccess::Write))
      m_execBarriers.recordCommands(m_cmd);

    // Discard the destination image if we're fully writing it,
    // and transition the image layout if necessary
    bool doDiscard = dstImage->isFullSubresource(region.dstSubresource, region.extent);

    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      doDiscard &= depthMode != VK_RESOLVE_MODE_NONE;
    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      doDiscard &= stencilMode != VK_RESOLVE_MODE_NONE;

    VkPipelineStageFlags dstStages;
    VkImageLayout dstLayout;
    VkAccessFlags dstAccess;

    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
      dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      dstStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    if (dstImage->info().layout != dstLayout || doDiscard) {
      m_execAcquires.accessImage(
        dstImage, dstSubresourceRange,
        doDiscard ? VK_IMAGE_LAYOUT_UNDEFINED
                  : dstImage->info().layout,
        dstImage->info().stages, 0,
        dstLayout, dstStages, dstAccess);
    }

    // Check source image layout, and try to avoid transitions if we can
    VkImageLayout srcLayout = srcImage->info().layout;
    
    if (srcLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     && srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
      srcLayout = (region.srcSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT & VK_IMAGE_ASPECT_COLOR_BIT)
        ? srcImage->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        : srcImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }
    
    if (srcImage->info().layout != srcLayout) {
      m_execAcquires.accessImage(
        srcImage, srcSubresourceRange,
        srcImage->info().layout,
        srcImage->info().stages, 0,
        srcLayout,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    }

    m_execAcquires.recordCommands(m_cmd);

    // Create a framebuffer and pipeline for the resolve op
    VkFormat dstFormat = format ? format : dstImage->info().format;
    VkFormat srcFormat = format ? format : srcImage->info().format;

    VkExtent3D passExtent = dstImage->mipLevelExtent(region.dstSubresource.mipLevel);

    DxvkMetaCopyViews views(
      dstImage, region.dstSubresource, dstFormat,
      srcImage, region.srcSubresource, srcFormat);

    DxvkMetaResolvePipeline pipeInfo = m_common->metaResolve().getPipeline(
      dstFormat, srcImage->info().sampleCount, depthMode, stencilMode);

    // Create and initialize descriptor set    
    VkDescriptorSet descriptorSet = m_descriptorPool->alloc(pipeInfo.dsetLayout);

    std::array<VkDescriptorImageInfo, 2> descriptorImages = {{
      { VK_NULL_HANDLE, views.srcImageView->handle(), srcLayout },
      { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },
    }};

    if (views.srcStencilView) {
      descriptorImages[1].imageView = views.srcStencilView->handle();
      descriptorImages[1].imageLayout = srcLayout;
    }

    std::array<VkWriteDescriptorSet, 2> descriptorWrites;

    for (uint32_t i = 0; i < descriptorWrites.size(); i++) {
      descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      descriptorWrites[i].dstSet = descriptorSet;
      descriptorWrites[i].dstBinding = i;
      descriptorWrites[i].descriptorCount = 1;
      descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      descriptorWrites[i].pImageInfo = &descriptorImages[i];
    }
    
    m_cmd->updateDescriptorSets(
      descriptorWrites.size(),
      descriptorWrites.data());

    // Set up render state    
    VkViewport viewport;
    viewport.x        = float(region.dstOffset.x);
    viewport.y        = float(region.dstOffset.y);
    viewport.width    = float(region.extent.width);
    viewport.height   = float(region.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset    = { region.dstOffset.x,  region.dstOffset.y   };
    scissor.extent    = { region.extent.width, region.extent.height };

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = views.dstImageView->handle();
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    if (doDiscard)
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { passExtent.width, passExtent.height };
    renderingInfo.layerCount = region.dstSubresource.layerCount;
    
    VkImageAspectFlags dstAspects = dstImage->formatInfo()->aspectMask;

    if (dstAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &attachmentInfo;
    } else {
      if (dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        renderingInfo.pDepthAttachment = &attachmentInfo;
      if (dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        renderingInfo.pStencilAttachment = &attachmentInfo;
    }

    // Perform the actual resolve operation
    VkOffset2D srcOffset = {
      region.srcOffset.x - region.dstOffset.x,
      region.srcOffset.y - region.dstOffset.y };
    
    m_cmd->cmdBeginRendering(&renderingInfo);
    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeHandle);
    m_cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);
    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(srcOffset), &srcOffset);
    m_cmd->cmdDraw(3, region.dstSubresource.layerCount, 0, 0);
    m_cmd->cmdEndRendering();

    m_execBarriers.accessImage(
      srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
      srcImage->info().layout,
      srcImage->info().stages,
      srcImage->info().access);

    m_execBarriers.accessImage(
      dstImage, dstSubresourceRange,
      dstLayout, dstStages, dstAccess,
      dstImage->info().layout,
      dstImage->info().stages,
      dstImage->info().access);

    m_cmd->trackResource<DxvkAccess::Write>(dstImage);
    m_cmd->trackResource<DxvkAccess::Read>(srcImage);
  }


  void DxvkContext::uploadImageFb(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset,
          VkFormat                  format) {
    if (!format)
      format = image->info().format;

    for (uint32_t i = 0; i < image->info().mipLevels; i++) {
      VkExtent3D mipExtent = image->mipLevelExtent(i);

      copyBufferToImageFb(image,
        vk::pickSubresourceLayers(image->getAvailableSubresources(), i),
        VkOffset3D { 0, 0, 0 }, mipExtent,
        source, sourceOffset, 0, 0, format);

      sourceOffset += image->info().numLayers * util::computeImageDataSize(
        format, mipExtent, image->formatInfo()->aspectMask);
    }
  }


  void DxvkContext::uploadImageHw(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset) {
    // Initialize all subresources of the image at once
    VkImageLayout transferLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    m_sdmaAcquires.accessImage(image,
      image->getAvailableSubresources(),
      VK_IMAGE_LAYOUT_UNDEFINED, 0, 0,
      transferLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT);

    m_sdmaAcquires.recordCommands(m_cmd);

    // Copy image data, one mip at a time
    VkDeviceSize dataOffset = sourceOffset;

    for (uint32_t i = 0; i < image->info().mipLevels; i++) {
      VkExtent3D mipExtent = image->mipLevelExtent(i);

      VkDeviceSize mipSize = image->info().numLayers * util::computeImageDataSize(
        image->info().format, mipExtent, image->formatInfo()->aspectMask);

      copyImageBufferData<true>(DxvkCmdBuffer::SdmaBuffer,
        image, vk::pickSubresourceLayers(image->getAvailableSubresources(), i),
        VkOffset3D { 0, 0, 0 }, mipExtent, transferLayout,
        source->getSliceHandle(dataOffset, mipSize), 0, 0);

      dataOffset += mipSize;
    }

    m_sdmaBarriers.releaseImage(m_initBarriers,
      image, image->getAvailableSubresources(),
      m_device->queues().transfer.queueFamily,
      transferLayout,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      m_device->queues().graphics.queueFamily,
      image->info().layout,
      image->info().stages,
      image->info().access);

    m_cmd->trackResource<DxvkAccess::Read>(source);
    m_cmd->trackResource<DxvkAccess::Write>(image);
  }


  void DxvkContext::startRenderPass() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      this->applyRenderTargetLoadLayouts();
      this->flushClears(true);

      // Make sure all graphics state gets reapplied on the next draw
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

      m_flags.set(
        DxvkContextFlag::GpRenderPassBound,
        DxvkContextFlag::GpDirtyPipeline,
        DxvkContextFlag::GpDirtyPipelineState,
        DxvkContextFlag::GpDirtyVertexBuffers,
        DxvkContextFlag::GpDirtyIndexBuffer,
        DxvkContextFlag::GpDirtyXfbBuffers,
        DxvkContextFlag::GpDirtyBlendConstants,
        DxvkContextFlag::GpDirtyStencilRef,
        DxvkContextFlag::GpDirtyMultisampleState,
        DxvkContextFlag::GpDirtyRasterizerState,
        DxvkContextFlag::GpDirtyViewport,
        DxvkContextFlag::GpDirtyDepthBias,
        DxvkContextFlag::GpDirtyDepthBounds,
        DxvkContextFlag::GpDirtyDepthStencilState,
        DxvkContextFlag::DirtyPushConstants);

      m_flags.clr(
        DxvkContextFlag::GpRenderPassSuspended,
        DxvkContextFlag::GpIndependentSets);

      this->renderPassBindFramebuffer(
        m_state.om.framebufferInfo,
        m_state.om.renderPassOps);

      // Track the final layout of each render target
      this->applyRenderTargetStoreLayouts();

      // Don't discard image contents if we have
      // to spill the current render pass
      this->resetRenderPassOps(
        m_state.om.renderTargets,
        m_state.om.renderPassOps);
      
      // Begin occlusion queries
      m_queryManager.beginQueries(m_cmd, VK_QUERY_TYPE_OCCLUSION);
      m_queryManager.beginQueries(m_cmd, VK_QUERY_TYPE_PIPELINE_STATISTICS);
    }
  }
  
  
  void DxvkContext::spillRenderPass(bool suspend) {
    if (m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      m_flags.clr(DxvkContextFlag::GpRenderPassBound);

      this->pauseTransformFeedback();
      
      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_OCCLUSION);
      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_PIPELINE_STATISTICS);
      
      this->renderPassUnbindFramebuffer();

      if (suspend)
        m_flags.set(DxvkContextFlag::GpRenderPassSuspended);
      else
        this->transitionRenderTargetLayouts(false);

      m_execBarriers.recordCommands(m_cmd);
    } else if (!suspend) {
      // We may end a previously suspended render pass
      if (m_flags.test(DxvkContextFlag::GpRenderPassSuspended)) {
        m_flags.clr(DxvkContextFlag::GpRenderPassSuspended);
        this->transitionRenderTargetLayouts(false);
        m_execBarriers.recordCommands(m_cmd);
      }

      // Execute deferred clears if necessary
      this->flushClears(false);
    }
  }


  void DxvkContext::renderPassEmitInitBarriers(
    const DxvkFramebufferInfo&  framebufferInfo,
    const DxvkRenderPassOps&    ops) {
    // If any of the involved images are dirty, emit all pending barriers now.
    // Otherwise, skip this step so that we can more efficiently batch barriers.
    for (uint32_t i = 0; i < framebufferInfo.numAttachments(); i++) {
      const auto& attachment = framebufferInfo.getAttachment(i);

      if (m_execBarriers.isImageDirty(
          attachment.view->image(),
          attachment.view->imageSubresources(),
          DxvkAccess::Write)) {
        m_execBarriers.recordCommands(m_cmd);
        break;
      }
    }

    // Transition all images to the render layout as necessary
    const auto& depthAttachment = framebufferInfo.getDepthTarget();

    if (depthAttachment.layout != ops.depthOps.loadLayout
     && depthAttachment.view != nullptr) {
      VkImageAspectFlags depthAspects = depthAttachment.view->info().aspects;

      VkPipelineStageFlags depthStages =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      VkAccessFlags depthAccess = 0;

      if (((depthAspects & VK_IMAGE_ASPECT_DEPTH_BIT) && ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_LOAD)
       || ((depthAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_LOAD))
        depthAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

      if (((depthAspects & VK_IMAGE_ASPECT_DEPTH_BIT) && ops.depthOps.loadOpD != VK_ATTACHMENT_LOAD_OP_LOAD)
       || ((depthAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && ops.depthOps.loadOpS != VK_ATTACHMENT_LOAD_OP_LOAD)
       || (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL))
        depthAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        depthStages |= m_device->getShaderPipelineStages();
        depthAccess |= VK_ACCESS_SHADER_READ_BIT;
      }

      m_execBarriers.accessImage(
        depthAttachment.view->image(),
        depthAttachment.view->imageSubresources(),
        ops.depthOps.loadLayout,
        depthStages, 0,
        depthAttachment.layout,
        depthStages, depthAccess);
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorAttachment = framebufferInfo.getColorTarget(i);

      if (colorAttachment.layout != ops.colorOps[i].loadLayout
       && colorAttachment.view != nullptr) {
        VkAccessFlags colorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        if (ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
          colorAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        m_execBarriers.accessImage(
          colorAttachment.view->image(),
          colorAttachment.view->imageSubresources(),
          ops.colorOps[i].loadLayout,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
          colorAttachment.layout,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          colorAccess);
      }
    }

    // Unconditionally emit barriers here. We need to do this
    // even if there are no layout transitions, since we don't
    // track resource usage during render passes.
    m_execBarriers.recordCommands(m_cmd);
  }


  void DxvkContext::renderPassEmitPostBarriers(
    const DxvkFramebufferInfo&  framebufferInfo,
    const DxvkRenderPassOps&    ops) {
    const auto& depthAttachment = framebufferInfo.getDepthTarget();

    if (depthAttachment.view != nullptr) {
      if (depthAttachment.layout != ops.depthOps.storeLayout) {
        m_execBarriers.accessImage(
          depthAttachment.view->image(),
          depthAttachment.view->imageSubresources(),
          depthAttachment.layout,
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          ops.depthOps.storeLayout,
          depthAttachment.view->image()->info().stages,
          depthAttachment.view->image()->info().access);
      } else {
        VkAccessFlags srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        if (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
          srcAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        m_execBarriers.accessMemory(
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          srcAccess,
          depthAttachment.view->image()->info().stages,
          depthAttachment.view->image()->info().access);
      }
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorAttachment = framebufferInfo.getColorTarget(i);

      if (colorAttachment.view != nullptr) {
        if (colorAttachment.layout != ops.colorOps[i].storeLayout) {
          m_execBarriers.accessImage(
            colorAttachment.view->image(),
            colorAttachment.view->imageSubresources(),
            colorAttachment.layout,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            ops.colorOps[i].storeLayout,
            colorAttachment.view->image()->info().stages,
            colorAttachment.view->image()->info().access);
        } else {
          m_execBarriers.accessMemory(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            colorAttachment.view->image()->info().stages,
            colorAttachment.view->image()->info().access);
        }
      }
    }

    // Do not flush barriers here. This is intended since
    // we pre-record them when binding the framebuffer.
  }


  void DxvkContext::renderPassBindFramebuffer(
    const DxvkFramebufferInfo&  framebufferInfo,
    const DxvkRenderPassOps&    ops) {
    const DxvkFramebufferSize fbSize = framebufferInfo.size();

    this->renderPassEmitInitBarriers(framebufferInfo, ops);
    this->renderPassEmitPostBarriers(framebufferInfo, ops);

    uint32_t colorInfoCount = 0;
    uint32_t lateClearCount = 0;

    std::array<VkRenderingAttachmentInfo, MaxNumRenderTargets> colorInfos;
    std::array<VkClearAttachment, MaxNumRenderTargets> lateClears;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorTarget = framebufferInfo.getColorTarget(i);
      colorInfos[i] = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

      if (colorTarget.view != nullptr) {
        colorInfos[i].imageView = colorTarget.view->handle();
        colorInfos[i].imageLayout = colorTarget.layout;
        colorInfos[i].loadOp = ops.colorOps[i].loadOp;
        colorInfos[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if (ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
          colorInfos[i].clearValue.color = ops.colorOps[i].clearValue;

          // We can't use LOAD_OP_CLEAR if the view format does not match the
          // underlying image format, so just discard here and use clear later.
          if (colorTarget.view->info().format != colorTarget.view->image()->info().format) {
            colorInfos[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

            auto& clear = lateClears[lateClearCount++];
            clear.colorAttachment = i;
            clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clear.clearValue.color = ops.colorOps[i].clearValue;
          }
        }

        colorInfoCount = i + 1;
      }
    }

    VkRenderingAttachmentInfo depthInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    VkImageAspectFlags depthStencilAspects = 0;

    if (framebufferInfo.getDepthTarget().view != nullptr) {
      const auto& depthTarget = framebufferInfo.getDepthTarget();
      depthStencilAspects = depthTarget.view->info().aspects;
      depthInfo.imageView = depthTarget.view->handle();
      depthInfo.imageLayout = depthTarget.layout;
      depthInfo.loadOp = ops.depthOps.loadOpD;
      depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      if (ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_CLEAR)
        depthInfo.clearValue.depthStencil.depth = ops.depthOps.clearValue.depth;
    }
    
    VkRenderingAttachmentInfo stencilInfo = depthInfo;

    if (framebufferInfo.getDepthTarget().view != nullptr) {
      stencilInfo.loadOp = ops.depthOps.loadOpS;
      stencilInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      if (ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_CLEAR)
        stencilInfo.clearValue.depthStencil.stencil = ops.depthOps.clearValue.stencil;
    }

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { fbSize.width, fbSize.height };
    renderingInfo.layerCount = fbSize.layers;

    if (colorInfoCount) {
      renderingInfo.colorAttachmentCount = colorInfoCount;
      renderingInfo.pColorAttachments = colorInfos.data();
    }

    if (depthStencilAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      renderingInfo.pDepthAttachment = &depthInfo;

    if (depthStencilAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      renderingInfo.pStencilAttachment = &stencilInfo;

    m_cmd->cmdBeginRendering(&renderingInfo);
    
    if (lateClearCount) {
      VkClearRect clearRect = { };
      clearRect.rect.extent.width   = fbSize.width;
      clearRect.rect.extent.height  = fbSize.height;
      clearRect.layerCount          = fbSize.layers;

      m_cmd->cmdClearAttachments(lateClearCount, lateClears.data(), 1, &clearRect);
    }

    for (uint32_t i = 0; i < framebufferInfo.numAttachments(); i++)
      m_cmd->trackResource<DxvkAccess::Write>(framebufferInfo.getAttachment(i).view->image());

    m_cmd->addStatCtr(DxvkStatCounter::CmdRenderPassCount, 1);
  }
  
  
  void DxvkContext::renderPassUnbindFramebuffer() {
    m_cmd->cmdEndRendering();

    // If there are pending layout transitions, execute them immediately
    // since the backend expects images to be in the store layout after
    // a render pass instance. This is expected to be rare.
    if (m_execBarriers.hasResourceBarriers())
      m_execBarriers.recordCommands(m_cmd);
  }
  
  
  void DxvkContext::resetRenderPassOps(
    const DxvkRenderTargets&    renderTargets,
          DxvkRenderPassOps&    renderPassOps) {
    if (renderTargets.depth.view != nullptr) {
      renderPassOps.depthOps = DxvkDepthAttachmentOps {
        VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD,
        renderTargets.depth.layout, renderTargets.depth.layout };
    } else {
      renderPassOps.depthOps = DxvkDepthAttachmentOps { };
    }
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (renderTargets.color[i].view != nullptr) {
        renderPassOps.colorOps[i] = DxvkColorAttachmentOps {
            VK_ATTACHMENT_LOAD_OP_LOAD,
            renderTargets.color[i].layout,
            renderTargets.color[i].layout };
      } else {
        renderPassOps.colorOps[i] = DxvkColorAttachmentOps { };
      }
    }
  }
  
  
  void DxvkContext::startTransformFeedback() {
    if (!m_flags.test(DxvkContextFlag::GpXfbActive)) {
      m_flags.set(DxvkContextFlag::GpXfbActive);

      VkBuffer     ctrBuffers[MaxNumXfbBuffers];
      VkDeviceSize ctrOffsets[MaxNumXfbBuffers];

      for (uint32_t i = 0; i < MaxNumXfbBuffers; i++) {
        m_state.xfb.activeCounters[i] = m_state.xfb.counters[i];
        auto physSlice = m_state.xfb.activeCounters[i].getSliceHandle();

        ctrBuffers[i] = physSlice.handle;
        ctrOffsets[i] = physSlice.offset;

        if (physSlice.handle != VK_NULL_HANDLE)
          m_cmd->trackResource<DxvkAccess::Read>(m_state.xfb.activeCounters[i].buffer());
      }
      
      m_cmd->cmdBeginTransformFeedback(
        0, MaxNumXfbBuffers, ctrBuffers, ctrOffsets);
      
      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT);
    }
  }


  void DxvkContext::pauseTransformFeedback() {
    if (m_flags.test(DxvkContextFlag::GpXfbActive)) {
      m_flags.clr(DxvkContextFlag::GpXfbActive);
      
      VkBuffer     ctrBuffers[MaxNumXfbBuffers];
      VkDeviceSize ctrOffsets[MaxNumXfbBuffers];

      for (uint32_t i = 0; i < MaxNumXfbBuffers; i++) {
        auto physSlice = m_state.xfb.activeCounters[i].getSliceHandle();

        ctrBuffers[i] = physSlice.handle;
        ctrOffsets[i] = physSlice.offset;

        if (physSlice.handle != VK_NULL_HANDLE)
          m_cmd->trackResource<DxvkAccess::Write>(m_state.xfb.activeCounters[i].buffer());

        m_state.xfb.activeCounters[i] = DxvkBufferSlice();
      }

      m_queryManager.endQueries(m_cmd, 
        VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT);
      
      m_cmd->cmdEndTransformFeedback(
        0, MaxNumXfbBuffers, ctrBuffers, ctrOffsets);
    }
  }


  void DxvkContext::unbindComputePipeline() {
    m_flags.set(DxvkContextFlag::CpDirtyPipelineState);

    m_state.cp.pipeline = nullptr;
  }
  
  
  bool DxvkContext::updateComputePipelineState() {
    if (unlikely(m_state.gp.pipeline != nullptr))
      this->unbindGraphicsPipeline();

    // Look up pipeline object based on the bound compute shader
    auto newPipeline = lookupComputePipeline(m_state.cp.shaders);
    m_state.cp.pipeline = newPipeline;

    if (unlikely(!newPipeline))
      return false;

    if (unlikely(newPipeline->getSpecConstantMask() != m_state.cp.constants.mask))
      this->resetSpecConstants<VK_PIPELINE_BIND_POINT_COMPUTE>(newPipeline->getSpecConstantMask());

    if (m_flags.test(DxvkContextFlag::CpDirtySpecConstants))
      this->updateSpecConstants<VK_PIPELINE_BIND_POINT_COMPUTE>();

    // Look up Vulkan pipeline handle for the given compute state
    auto pipelineHandle = newPipeline->getPipelineHandle(m_state.cp.state);
    
    if (unlikely(!pipelineHandle))
      return false;

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipelineHandle);

    // Mark compute resources and push constants as dirty
    m_descriptorState.dirtyStages(VK_SHADER_STAGE_COMPUTE_BIT);

    if (newPipeline->getBindings()->layout().getPushConstantRange(true).size)
      m_flags.set(DxvkContextFlag::DirtyPushConstants);

    m_flags.clr(DxvkContextFlag::CpDirtyPipelineState);
    return true;
  }
  
  
  void DxvkContext::unbindGraphicsPipeline() {
    m_flags.set(
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::GpDirtyXfbBuffers,
      DxvkContextFlag::GpDirtyBlendConstants,
      DxvkContextFlag::GpDirtyStencilRef,
      DxvkContextFlag::GpDirtyMultisampleState,
      DxvkContextFlag::GpDirtyRasterizerState,
      DxvkContextFlag::GpDirtyViewport,
      DxvkContextFlag::GpDirtyDepthBias,
      DxvkContextFlag::GpDirtyDepthBounds,
      DxvkContextFlag::GpDirtyDepthStencilState);

    m_state.gp.pipeline = nullptr;
  }
  
  
  bool DxvkContext::updateGraphicsPipeline() {
    if (unlikely(m_state.cp.pipeline != nullptr))
      this->unbindComputePipeline();

    auto newPipeline = lookupGraphicsPipeline(m_state.gp.shaders);
    m_state.gp.pipeline = newPipeline;

    if (unlikely(!newPipeline)) {
      m_state.gp.flags = DxvkGraphicsPipelineFlags();
      return false;
    }

    if (m_features.test(DxvkContextFeature::TrackGraphicsPipeline))
      m_cmd->trackGraphicsPipeline(newPipeline);

    if (unlikely(newPipeline->getSpecConstantMask() != m_state.gp.constants.mask))
      this->resetSpecConstants<VK_PIPELINE_BIND_POINT_GRAPHICS>(newPipeline->getSpecConstantMask());

    DxvkGraphicsPipelineFlags oldFlags = m_state.gp.flags;
    DxvkGraphicsPipelineFlags newFlags = newPipeline->flags();
    DxvkGraphicsPipelineFlags diffFlags = oldFlags ^ newFlags;

    DxvkGraphicsPipelineFlags hazardMask(
      DxvkGraphicsPipelineFlag::HasTransformFeedback,
      DxvkGraphicsPipelineFlag::HasStorageDescriptors);

    m_state.gp.flags = newFlags;

    if ((diffFlags & hazardMask) != 0) {
      // Force-update vertex/index buffers for hazard checks
      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer,
                  DxvkContextFlag::GpDirtyVertexBuffers,
                  DxvkContextFlag::GpDirtyXfbBuffers,
                  DxvkContextFlag::DirtyDrawBuffer);

      // This is necessary because we'll only do hazard
      // tracking if the active pipeline has side effects
      if (!m_barrierControl.test(DxvkBarrierControl::IgnoreGraphicsBarriers))
        this->spillRenderPass(true);
    }

    if (diffFlags.test(DxvkGraphicsPipelineFlag::HasSampleMaskExport))
      m_flags.set(DxvkContextFlag::GpDirtyMultisampleState);

    m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

    if (newPipeline->getBindings()->layout().getPushConstantRange(true).size)
      m_flags.set(DxvkContextFlag::DirtyPushConstants);

    m_flags.clr(DxvkContextFlag::GpDirtyPipeline);
    return true;
  }
  
  
  bool DxvkContext::updateGraphicsPipelineState(DxvkGlobalPipelineBarrier srcBarrier) {
    bool oldIndependentSets = m_flags.test(DxvkContextFlag::GpIndependentSets);

    // Check which dynamic states need to be active. States that
    // are not dynamic will be invalidated in the command buffer.
    m_flags.clr(DxvkContextFlag::GpDynamicBlendConstants,
                DxvkContextFlag::GpDynamicDepthStencilState,
                DxvkContextFlag::GpDynamicDepthBias,
                DxvkContextFlag::GpDynamicDepthBounds,
                DxvkContextFlag::GpDynamicStencilRef,
                DxvkContextFlag::GpDynamicMultisampleState,
                DxvkContextFlag::GpDynamicRasterizerState,
                DxvkContextFlag::GpIndependentSets);
    
    m_flags.set(m_state.gp.state.useDynamicBlendConstants()
      ? DxvkContextFlag::GpDynamicBlendConstants
      : DxvkContextFlag::GpDirtyBlendConstants);
    
    m_flags.set((!m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasRasterizerDiscard))
      ? DxvkContextFlag::GpDynamicRasterizerState
      : DxvkContextFlag::GpDirtyRasterizerState);

    // Retrieve and bind actual Vulkan pipeline handle
    auto pipelineInfo = m_state.gp.pipeline->getPipelineHandle(m_state.gp.state);

    if (unlikely(!pipelineInfo.first))
      return false;

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineInfo.first);

    // For pipelines created from graphics pipeline libraries, we need to
    // apply a bunch of dynamic state that is otherwise static or unused
    if (pipelineInfo.second == DxvkGraphicsPipelineType::BasePipeline) {
      m_flags.set(
        DxvkContextFlag::GpDynamicDepthStencilState,
        DxvkContextFlag::GpDynamicDepthBias,
        DxvkContextFlag::GpDynamicStencilRef,
        DxvkContextFlag::GpIndependentSets);

      if (m_device->features().core.features.depthBounds)
        m_flags.set(DxvkContextFlag::GpDynamicDepthBounds);

      if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasSampleRateShading)
       && m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
       && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask)
        m_flags.set(DxvkContextFlag::GpDynamicMultisampleState);
    } else {
      m_flags.set(m_state.gp.state.useDynamicDepthBias()
        ? DxvkContextFlag::GpDynamicDepthBias
        : DxvkContextFlag::GpDirtyDepthBias);

      m_flags.set(m_state.gp.state.useDynamicDepthBounds()
        ? DxvkContextFlag::GpDynamicDepthBounds
        : DxvkContextFlag::GpDirtyDepthBounds);

      m_flags.set(m_state.gp.state.useDynamicStencilRef()
        ? DxvkContextFlag::GpDynamicStencilRef
        : DxvkContextFlag::GpDirtyStencilRef);

      m_flags.set(
        DxvkContextFlag::GpDirtyDepthStencilState,
        DxvkContextFlag::GpDirtyMultisampleState);
    }

    // If necessary, dirty descriptor sets due to layout incompatibilities
    bool newIndependentSets = m_flags.test(DxvkContextFlag::GpIndependentSets);

    if (newIndependentSets != oldIndependentSets)
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

    // Emit barrier based on pipeline properties, in order to avoid
    // accidental write-after-read hazards after the render pass.
    DxvkGlobalPipelineBarrier pipelineBarrier = m_state.gp.pipeline->getGlobalBarrier(m_state.gp.state);
    srcBarrier.stages |= pipelineBarrier.stages;
    srcBarrier.access |= pipelineBarrier.access;

    if (srcBarrier.stages) {
      DxvkAccessFlags access = DxvkBarrierSet::getAccessTypes(srcBarrier.access);
      DxvkGlobalPipelineBarrier dstBarrier = access.test(DxvkAccess::Write)
        ? m_globalRwGraphicsBarrier
        : m_globalRoGraphicsBarrier;

      m_execBarriers.accessMemory(
        srcBarrier.stages, srcBarrier.access,
        dstBarrier.stages, dstBarrier.access);
    }

    m_flags.clr(DxvkContextFlag::GpDirtyPipelineState);
    return true;
  }


  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::resetSpecConstants(
          uint32_t                newMask) {
    auto& scInfo  = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? m_state.gp.state.sc  : m_state.cp.state.sc;
    auto& scState = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? m_state.gp.constants : m_state.cp.constants;

    // Set all constants to 0 that were used by the previous pipeline
    // but are not used by the old one. Any stale data could otherwise
    // lead to unnecessary pipeline variants being created.
    for (auto i : bit::BitMask(scState.mask & ~newMask))
      scInfo.specConstants[i] = 0;

    scState.mask = newMask;

    auto flag = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
      ? DxvkContextFlag::GpDirtySpecConstants
      : DxvkContextFlag::CpDirtySpecConstants;

    if (newMask)
      m_flags.set(flag);
    else
      m_flags.clr(flag);
  }


  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::updateSpecConstants() {
    auto& scInfo  = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? m_state.gp.state.sc  : m_state.cp.state.sc;
    auto& scState = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? m_state.gp.constants : m_state.cp.constants;

    for (auto i : bit::BitMask(scState.mask))
      scInfo.specConstants[i] = scState.data[i];

    if (BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      m_flags.clr(DxvkContextFlag::GpDirtySpecConstants);
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    } else {
      m_flags.clr(DxvkContextFlag::CpDirtySpecConstants);
      m_flags.set(DxvkContextFlag::CpDirtyPipelineState);
    }
  }


  void DxvkContext::invalidateState() {
    this->unbindComputePipeline();
    this->unbindGraphicsPipeline();
  }

  
  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::updateResourceBindings(const DxvkBindingLayoutObjects* layout) {
    const auto& bindings = layout->layout();

    // Ensure that the arrays we write descriptor info to are big enough
    if (unlikely(layout->getBindingCount() > m_descriptors.size()))
      this->resizeDescriptorArrays(layout->getBindingCount());

    // On 32-bit wine, vkUpdateDescriptorSets has significant overhead due
    // to struct conversion, so we should use descriptor update templates.
    // For 64-bit applications, using templates is slower on some drivers.
    constexpr bool useDescriptorTemplates = env::is32BitHostPlatform();

    bool independentSets = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
                        && m_flags.test(DxvkContextFlag::GpIndependentSets);

    uint32_t layoutSetMask = layout->getSetMask();
    uint32_t dirtySetMask = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
      ? m_descriptorState.getDirtyGraphicsSets()
      : m_descriptorState.getDirtyComputeSets();
    dirtySetMask &= layoutSetMask;

    std::array<VkDescriptorSet, DxvkDescriptorSets::SetCount> sets;
    m_descriptorPool->alloc(layout, dirtySetMask, sets.data());

    uint32_t descriptorCount = 0;

    for (auto setIndex : bit::BitMask(dirtySetMask)) {
      uint32_t bindingCount = bindings.getBindingCount(setIndex);
      VkDescriptorSet set = sets[setIndex];

      for (uint32_t j = 0; j < bindingCount; j++) {
        const auto& binding = bindings.getBinding(setIndex, j);

        if (!useDescriptorTemplates) {
          auto& descriptorWrite = m_descriptorWrites[descriptorCount];
          descriptorWrite.dstSet = set;
          descriptorWrite.dstBinding = j;
          descriptorWrite.descriptorType = binding.descriptorType;
        }

        auto& descriptorInfo = m_descriptors[descriptorCount++];

        switch (binding.descriptorType) {
          case VK_DESCRIPTOR_TYPE_SAMPLER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.sampler != nullptr) {
              descriptorInfo.image.sampler = res.sampler->handle();
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackSampler(res.sampler);
            } else {
              descriptorInfo.image.sampler = m_common->dummyResources().samplerHandle();
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.imageView != nullptr && res.imageView->handle(binding.viewType) != VK_NULL_HANDLE) {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = res.imageView->handle(binding.viewType);
              descriptorInfo.image.imageLayout = res.imageView->image()->info().layout;

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Read>(res.imageView->image());
            } else {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.imageView != nullptr && res.imageView->handle(binding.viewType) != VK_NULL_HANDLE) {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = res.imageView->handle(binding.viewType);
              descriptorInfo.image.imageLayout = res.imageView->image()->info().layout;

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Write>(res.imageView->image());
            } else {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.sampler != nullptr && res.imageView != nullptr
            && res.imageView->handle(binding.viewType) != VK_NULL_HANDLE) {
              descriptorInfo.image.sampler = res.sampler->handle();
              descriptorInfo.image.imageView = res.imageView->handle(binding.viewType);
              descriptorInfo.image.imageLayout = res.imageView->image()->info().layout;

              if (m_rcTracked.set(binding.resourceBinding)) {
                m_cmd->trackSampler(res.sampler);
                m_cmd->trackResource<DxvkAccess::Read>(res.imageView->image());
              }
            } else {
              descriptorInfo.image.sampler = m_common->dummyResources().samplerHandle();
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferView != nullptr) {
              descriptorInfo.texelBuffer = res.bufferView->handle();

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Read>(res.bufferView->buffer());
            } else {
              descriptorInfo.texelBuffer = VK_NULL_HANDLE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferView != nullptr) {
              descriptorInfo.texelBuffer = res.bufferView->handle();

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Write>(res.bufferView->buffer());
            } else {
              descriptorInfo.texelBuffer = VK_NULL_HANDLE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferSlice.length()) {
              descriptorInfo = res.bufferSlice.getDescriptor();

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Read>(res.bufferSlice.buffer());
            } else {
              descriptorInfo.buffer.buffer = VK_NULL_HANDLE;
              descriptorInfo.buffer.offset = 0;
              descriptorInfo.buffer.range = VK_WHOLE_SIZE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferSlice.length()) {
              descriptorInfo = res.bufferSlice.getDescriptor();

              if (m_rcTracked.set(binding.resourceBinding))
                m_cmd->trackResource<DxvkAccess::Write>(res.bufferSlice.buffer());
            } else {
              descriptorInfo.buffer.buffer = VK_NULL_HANDLE;
              descriptorInfo.buffer.offset = 0;
              descriptorInfo.buffer.range = VK_WHOLE_SIZE;
            }
          } break;

          default:
            break;
        }
      }

      if (useDescriptorTemplates) {
        m_cmd->updateDescriptorSetWithTemplate(set,
          layout->getSetUpdateTemplate(setIndex),
          &m_descriptors[0]);
        descriptorCount = 0;
      }

      // If the next set is not dirty, update and bind all previously
      // updated sets in one go in order to reduce api call overhead.
      if (!(((dirtySetMask >> 1) >> setIndex) & 1u)) {
        if (!useDescriptorTemplates) {
          m_cmd->updateDescriptorSets(descriptorCount,
            m_descriptorWrites.data());
          descriptorCount = 0;
        }

        // Find first dirty set in the mask and clear bits
        // for all sets that we're going to update here.
        uint32_t firstSet = bit::tzcnt(dirtySetMask);
        dirtySetMask &= (~1u) << setIndex;

        m_cmd->cmdBindDescriptorSets(DxvkCmdBuffer::ExecBuffer,
          BindPoint, layout->getPipelineLayout(independentSets),
          firstSet, setIndex - firstSet + 1, &sets[firstSet],
          0, nullptr);
      }
    }
  }


  void DxvkContext::updateComputeShaderResources() {
    this->updateResourceBindings<VK_PIPELINE_BIND_POINT_COMPUTE>(m_state.cp.pipeline->getBindings());

    m_descriptorState.clearStages(VK_SHADER_STAGE_COMPUTE_BIT);
  }
  
  
  void DxvkContext::updateGraphicsShaderResources() {
    this->updateResourceBindings<VK_PIPELINE_BIND_POINT_GRAPHICS>(m_state.gp.pipeline->getBindings());

    m_descriptorState.clearStages(VK_SHADER_STAGE_ALL_GRAPHICS);
  }
  
  
  DxvkFramebufferInfo DxvkContext::makeFramebufferInfo(
    const DxvkRenderTargets&      renderTargets) {
    return DxvkFramebufferInfo(renderTargets, m_device->getDefaultFramebufferSize());
  }


  void DxvkContext::updateFramebuffer() {
    if (m_flags.test(DxvkContextFlag::GpDirtyFramebuffer)) {
      m_flags.clr(DxvkContextFlag::GpDirtyFramebuffer);

      this->spillRenderPass(true);

      DxvkFramebufferInfo fbInfo = makeFramebufferInfo(m_state.om.renderTargets);
      this->updateRenderTargetLayouts(fbInfo, m_state.om.framebufferInfo);

      // Update relevant graphics pipeline state
      m_state.gp.state.ms.setSampleCount(fbInfo.getSampleCount());
      m_state.gp.state.rt = fbInfo.getRtInfo();
      m_state.om.framebufferInfo = fbInfo;

      for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
        const Rc<DxvkImageView>& attachment = fbInfo.getColorTarget(i).view;

        VkComponentMapping mapping = attachment != nullptr
          ? util::invertComponentMapping(attachment->info().unpackSwizzle())
          : VkComponentMapping();

        m_state.gp.state.omSwizzle[i] = DxvkOmAttachmentSwizzle(mapping);
      }

      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    }
  }


  void DxvkContext::applyRenderTargetLoadLayouts() {
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      m_state.om.renderPassOps.colorOps[i].loadLayout = m_rtLayouts.color[i];

    m_state.om.renderPassOps.depthOps.loadLayout = m_rtLayouts.depth;
  }


  void DxvkContext::applyRenderTargetStoreLayouts() {
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      m_rtLayouts.color[i] = m_state.om.renderPassOps.colorOps[i].storeLayout;

    m_rtLayouts.depth = m_state.om.renderPassOps.depthOps.storeLayout;
  }


  void DxvkContext::transitionRenderTargetLayouts(
          bool                    sharedOnly) {
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const DxvkAttachment& color = m_state.om.framebufferInfo.getColorTarget(i);

      if (color.view != nullptr && (!sharedOnly || color.view->image()->info().shared)) {
        this->transitionColorAttachment(color, m_rtLayouts.color[i]);
        m_rtLayouts.color[i] = color.view->image()->info().layout;
      }
    }

    const DxvkAttachment& depth = m_state.om.framebufferInfo.getDepthTarget();

    if (depth.view != nullptr && (!sharedOnly || depth.view->image()->info().shared)) {
      this->transitionDepthAttachment(depth, m_rtLayouts.depth);
      m_rtLayouts.depth = depth.view->image()->info().layout;
    }
  }


  void DxvkContext::transitionColorAttachment(
    const DxvkAttachment&         attachment,
          VkImageLayout           oldLayout) {
    if (oldLayout != attachment.view->image()->info().layout) {
      m_execBarriers.accessImage(
        attachment.view->image(),
        attachment.view->imageSubresources(), oldLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        attachment.view->image()->info().layout,
        attachment.view->image()->info().stages,
        attachment.view->image()->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(attachment.view->image());
    }
  }


  void DxvkContext::transitionDepthAttachment(
    const DxvkAttachment&         attachment,
          VkImageLayout           oldLayout) {
    if (oldLayout != attachment.view->image()->info().layout) {
      m_execBarriers.accessImage(
        attachment.view->image(),
        attachment.view->imageSubresources(), oldLayout,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        oldLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
          ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0,
        attachment.view->image()->info().layout,
        attachment.view->image()->info().stages,
        attachment.view->image()->info().access);

      m_cmd->trackResource<DxvkAccess::Write>(attachment.view->image());
    }
  }


  void DxvkContext::updateRenderTargetLayouts(
    const DxvkFramebufferInfo&    newFb,
    const DxvkFramebufferInfo&    oldFb) {
    DxvkRenderTargetLayouts layouts = { };

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (newFb.getColorTarget(i).view != nullptr)
        layouts.color[i] = newFb.getColorTarget(i).view->image()->info().layout;
    }

    if (newFb.getDepthTarget().view != nullptr)
      layouts.depth = newFb.getDepthTarget().view->image()->info().layout;

    // Check whether any of the previous attachments have been moved
    // around or been rebound with a different view. This may help
    // reduce the number of image layout transitions between passes.
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const DxvkAttachment& oldAttachment = oldFb.getColorTarget(i);

      if (oldAttachment.view != nullptr) {
        bool found = false;

        for (uint32_t j = 0; j < MaxNumRenderTargets && !found; j++) {
          const DxvkAttachment& newAttachment = newFb.getColorTarget(j);

          found = newAttachment.view == oldAttachment.view || (newAttachment.view != nullptr
            && newAttachment.view->image()        == oldAttachment.view->image()
            && newAttachment.view->subresources() == oldAttachment.view->subresources());

          if (found)
            layouts.color[j] = m_rtLayouts.color[i];
        }

        if (!found && m_flags.test(DxvkContextFlag::GpRenderPassSuspended))
          this->transitionColorAttachment(oldAttachment, m_rtLayouts.color[i]);
      }
    }

    const DxvkAttachment& oldAttachment = oldFb.getDepthTarget();

    if (oldAttachment.view != nullptr) {
      const DxvkAttachment& newAttachment = newFb.getDepthTarget();

      bool found = newAttachment.view == oldAttachment.view || (newAttachment.view != nullptr
        && newAttachment.view->image()        == oldAttachment.view->image()
        && newAttachment.view->subresources() == oldAttachment.view->subresources());

      if (found)
        layouts.depth = m_rtLayouts.depth;
      else if (m_flags.test(DxvkContextFlag::GpRenderPassSuspended))
        this->transitionDepthAttachment(oldAttachment, m_rtLayouts.depth);
    }

    m_rtLayouts = layouts;
  }
  
  
  void DxvkContext::prepareImage(
    const Rc<DxvkImage>&          image,
    const VkImageSubresourceRange& subresources,
          bool                    flushClears) {
    // Images that can't be used as attachments are always in their
    // default layout, so we don't have to do anything in this case
    if (!(image->info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)))
      return;

    // Flush clears if there are any since they may affect the image
    if (!m_deferredClears.empty() && flushClears)
      this->spillRenderPass(false);

    // All images are in their default layout for suspended passes
    if (!m_flags.test(DxvkContextFlag::GpRenderPassSuspended))
      return;

    // 3D images require special care because they only have one
    // layer, but views may address individual 2D slices as layers
    bool is3D = image->info().type == VK_IMAGE_TYPE_3D;

    // Transition any attachment with overlapping subresources
    if (image->info().usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
        const DxvkAttachment& attachment = m_state.om.framebufferInfo.getColorTarget(i);

        if (attachment.view != nullptr && attachment.view->image() == image
         && (is3D || vk::checkSubresourceRangeOverlap(attachment.view->subresources(), subresources))) {
          this->transitionColorAttachment(attachment, m_rtLayouts.color[i]);
          m_rtLayouts.color[i] = image->info().layout;
        }
      }
    } else {
      const DxvkAttachment& attachment = m_state.om.framebufferInfo.getDepthTarget();

      if (attachment.view != nullptr && attachment.view->image() == image
       && (is3D || vk::checkSubresourceRangeOverlap(attachment.view->subresources(), subresources))) {
        this->transitionDepthAttachment(attachment, m_rtLayouts.depth);
        m_rtLayouts.depth = image->info().layout;
      }
    }
  }


  bool DxvkContext::updateIndexBufferBinding() {
    if (unlikely(!m_state.vi.indexBuffer.length()))
      return false;

    m_flags.clr(DxvkContextFlag::GpDirtyIndexBuffer);
    auto bufferInfo = m_state.vi.indexBuffer.getDescriptor();

    if (m_features.test(DxvkContextFeature::IndexBufferRobustness)) {
      VkDeviceSize align = m_state.vi.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
      VkDeviceSize range = bufferInfo.buffer.range & ~(align - 1);

      m_cmd->cmdBindIndexBuffer2(
        bufferInfo.buffer.buffer,
        bufferInfo.buffer.offset,
        range, m_state.vi.indexType);
    } else {
      m_cmd->cmdBindIndexBuffer(
        bufferInfo.buffer.buffer,
        bufferInfo.buffer.offset,
        m_state.vi.indexType);
    }

    if (m_vbTracked.set(MaxNumVertexBindings))
      m_cmd->trackResource<DxvkAccess::Read>(m_state.vi.indexBuffer.buffer());

    return true;
  }
  
  
  void DxvkContext::updateVertexBufferBindings() {
    m_flags.clr(DxvkContextFlag::GpDirtyVertexBuffers);

    if (unlikely(!m_state.gp.state.il.bindingCount()))
      return;
    
    std::array<VkBuffer,     MaxNumVertexBindings> buffers;
    std::array<VkDeviceSize, MaxNumVertexBindings> offsets;
    std::array<VkDeviceSize, MaxNumVertexBindings> lengths;
    std::array<VkDeviceSize, MaxNumVertexBindings> strides;
    
    bool oldDynamicStrides = m_flags.test(DxvkContextFlag::GpDynamicVertexStrides);
    bool newDynamicStrides = true;

    // Set buffer handles and offsets for active bindings
    for (uint32_t i = 0; i < m_state.gp.state.il.bindingCount(); i++) {
      uint32_t binding = m_state.gp.state.ilBindings[i].binding();
      
      if (likely(m_state.vi.vertexBuffers[binding].length())) {
        auto vbo = m_state.vi.vertexBuffers[binding].getDescriptor();
        
        buffers[i] = vbo.buffer.buffer;
        offsets[i] = vbo.buffer.offset;
        lengths[i] = vbo.buffer.range;
        strides[i] = m_state.vi.vertexStrides[binding];

        if (strides[i]) {
          // Dynamic strides are only allowed if the stride is not smaller
          // than highest attribute offset + format size for given binding
          newDynamicStrides &= strides[i] >= m_state.vi.vertexExtents[i];
        }

        if (m_vbTracked.set(binding))
          m_cmd->trackResource<DxvkAccess::Read>(m_state.vi.vertexBuffers[binding].buffer());
      } else {
        buffers[i] = VK_NULL_HANDLE;
        offsets[i] = 0;
        lengths[i] = 0;
        strides[i] = 0;
      }
    }

    // If vertex strides are static or if we are switching between static or
    // dynamic strides, we'll have to apply them to the pipeline state and
    // also sort out our state flags
    if (unlikely(!oldDynamicStrides) || unlikely(!newDynamicStrides)) {
      m_flags.clr(DxvkContextFlag::GpDynamicVertexStrides);

      for (uint32_t i = 0; i < m_state.gp.state.il.bindingCount(); i++) {
        uint32_t stride = newDynamicStrides ? 0 : strides[i];

        if (m_state.gp.state.ilBindings[i].stride() != stride) {
          m_state.gp.state.ilBindings[i].setStride(stride);
          m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
        }
      }

      if (newDynamicStrides)
        m_flags.set(DxvkContextFlag::GpDynamicVertexStrides);
    }

    // Vertex bindigs get remapped when compiling the
    // pipeline, so this actually does the right thing
    m_cmd->cmdBindVertexBuffers(0, m_state.gp.state.il.bindingCount(),
      buffers.data(), offsets.data(), lengths.data(),
      newDynamicStrides ? strides.data() : nullptr);
  }
  
  
  void DxvkContext::updateTransformFeedbackBuffers() {
    const auto& gsInfo = m_state.gp.shaders.gs->info();

    VkBuffer     xfbBuffers[MaxNumXfbBuffers];
    VkDeviceSize xfbOffsets[MaxNumXfbBuffers];
    VkDeviceSize xfbLengths[MaxNumXfbBuffers];

    for (size_t i = 0; i < MaxNumXfbBuffers; i++) {
      auto physSlice = m_state.xfb.buffers[i].getSliceHandle();
      
      xfbBuffers[i] = physSlice.handle;
      xfbOffsets[i] = physSlice.offset;
      xfbLengths[i] = physSlice.length;

      if (physSlice.handle == VK_NULL_HANDLE)
        xfbBuffers[i] = m_common->dummyResources().bufferHandle();
      
      if (physSlice.handle != VK_NULL_HANDLE) {
        const Rc<DxvkBuffer>& buffer = m_state.xfb.buffers[i].buffer();
        buffer->setXfbVertexStride(gsInfo.xfbStrides[i]);
        
        m_cmd->trackResource<DxvkAccess::Write>(buffer);
      }
    }

    m_cmd->cmdBindTransformFeedbackBuffers(
      0, MaxNumXfbBuffers,
      xfbBuffers, xfbOffsets, xfbLengths);
  }


  void DxvkContext::updateTransformFeedbackState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyXfbBuffers)) {
      m_flags.clr(DxvkContextFlag::GpDirtyXfbBuffers);

      this->pauseTransformFeedback();
      this->updateTransformFeedbackBuffers();
    }

    this->startTransformFeedback();
  }

  
  void DxvkContext::updateDynamicState() {
    if (unlikely(m_flags.test(DxvkContextFlag::GpDirtyViewport))) {
      m_flags.clr(DxvkContextFlag::GpDirtyViewport);

      m_cmd->cmdSetViewport(m_state.vp.viewportCount, m_state.vp.viewports.data());
      m_cmd->cmdSetScissor(m_state.vp.viewportCount, m_state.vp.scissorRects.data());
    }

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtyDepthStencilState,
                             DxvkContextFlag::GpDynamicDepthStencilState))) {
      m_flags.clr(DxvkContextFlag::GpDirtyDepthStencilState);

      // Make sure to not enable writes to aspects that cannot be
      // written in the current depth-stencil attachment layout.
      // This mirrors what we do for monolithic pipelines.
      VkImageAspectFlags dsReadOnlyAspects = m_state.gp.state.rt.getDepthStencilReadOnlyAspects();

      bool enableDepthWrites = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
      bool enableStencilWrites = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

      m_cmd->cmdSetDepthState(
        m_state.gp.state.ds.enableDepthTest(),
        m_state.gp.state.ds.enableDepthWrite() && enableDepthWrites,
        m_state.gp.state.ds.depthCompareOp());

      if (m_device->features().core.features.depthBounds) {
        m_cmd->cmdSetDepthBoundsState(
          m_state.gp.state.ds.enableDepthBoundsTest());

        m_flags.set(DxvkContextFlag::GpDynamicDepthBounds);
      }

      m_cmd->cmdSetStencilState(
        m_state.gp.state.ds.enableStencilTest(),
        m_state.gp.state.dsFront.state(enableStencilWrites),
        m_state.gp.state.dsBack.state(enableStencilWrites));

      m_cmd->cmdSetDepthBiasState(
        m_state.gp.state.rs.depthBiasEnable());

      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable) {
        m_cmd->cmdSetDepthClipState(
          m_state.gp.state.rs.depthClipEnable());
      }
    }

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtyMultisampleState,
                             DxvkContextFlag::GpDynamicMultisampleState))) {
      m_flags.clr(DxvkContextFlag::GpDirtyMultisampleState);

      // Infer actual sample count from both the multisample state
      // and rasterizer state, just like during pipeline creation
      VkSampleCountFlagBits sampleCount = VkSampleCountFlagBits(m_state.gp.state.ms.sampleCount());

      if (!sampleCount) {
        sampleCount = m_state.gp.state.rs.sampleCount()
          ? VkSampleCountFlagBits(m_state.gp.state.rs.sampleCount())
          : VK_SAMPLE_COUNT_1_BIT;
      }

      VkSampleMask sampleMask = m_state.gp.state.ms.sampleMask() & ((1u << sampleCount) - 1u);
      m_cmd->cmdSetMultisampleState(sampleCount, sampleMask);

      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable
       && !m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasSampleMaskExport))
        m_cmd->cmdSetAlphaToCoverageState(m_state.gp.state.ms.enableAlphaToCoverage());
    }

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtyBlendConstants,
                             DxvkContextFlag::GpDynamicBlendConstants))) {
      m_flags.clr(DxvkContextFlag::GpDirtyBlendConstants);
      m_cmd->cmdSetBlendConstants(&m_state.dyn.blendConstants.r);
    }

    if (m_flags.all(DxvkContextFlag::GpDirtyRasterizerState,
                    DxvkContextFlag::GpDynamicRasterizerState)) {
      m_flags.clr(DxvkContextFlag::GpDirtyRasterizerState);

      m_cmd->cmdSetRasterizerState(
        m_state.dyn.cullMode,
        m_state.dyn.frontFace);
    }

    if (m_flags.all(DxvkContextFlag::GpDirtyStencilRef,
                    DxvkContextFlag::GpDynamicStencilRef)) {
      m_flags.clr(DxvkContextFlag::GpDirtyStencilRef);

      m_cmd->cmdSetStencilReference(
        VK_STENCIL_FRONT_AND_BACK,
        m_state.dyn.stencilReference);
    }
    
    if (m_flags.all(DxvkContextFlag::GpDirtyDepthBias,
                    DxvkContextFlag::GpDynamicDepthBias)) {
      m_flags.clr(DxvkContextFlag::GpDirtyDepthBias);

      if (m_device->features().extDepthBiasControl.depthBiasControl) {
        VkDepthBiasRepresentationInfoEXT depthBiasRepresentation = { VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT };
        depthBiasRepresentation.depthBiasRepresentation = m_state.dyn.depthBiasRepresentation.depthBiasRepresentation;
        depthBiasRepresentation.depthBiasExact          = m_state.dyn.depthBiasRepresentation.depthBiasExact;

        VkDepthBiasInfoEXT depthBiasInfo = { VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT };
        depthBiasInfo.pNext                   = &depthBiasRepresentation;
        depthBiasInfo.depthBiasConstantFactor = m_state.dyn.depthBias.depthBiasConstant;
        depthBiasInfo.depthBiasClamp          = m_state.dyn.depthBias.depthBiasClamp;
        depthBiasInfo.depthBiasSlopeFactor    = m_state.dyn.depthBias.depthBiasSlope;

        m_cmd->cmdSetDepthBias2(&depthBiasInfo);
      } else {
        m_cmd->cmdSetDepthBias(
          m_state.dyn.depthBias.depthBiasConstant,
          m_state.dyn.depthBias.depthBiasClamp,
          m_state.dyn.depthBias.depthBiasSlope);
      }
    }
    
    if (m_flags.all(DxvkContextFlag::GpDirtyDepthBounds,
                    DxvkContextFlag::GpDynamicDepthBounds)) {
      m_flags.clr(DxvkContextFlag::GpDirtyDepthBounds);

      m_cmd->cmdSetDepthBounds(
        m_state.dyn.depthBounds.minDepthBounds,
        m_state.dyn.depthBounds.maxDepthBounds);
    }
  }


  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::updatePushConstants() {
    m_flags.clr(DxvkContextFlag::DirtyPushConstants);

    auto bindings = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
      ? m_state.gp.pipeline->getBindings()
      : m_state.cp.pipeline->getBindings();

    // Optimized pipelines may have push constants trimmed, so look up
    // the exact layout used for the currently bound pipeline.
    bool independentSets = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
      && m_flags.test(DxvkContextFlag::GpIndependentSets);

    VkPushConstantRange pushConstRange = bindings->layout().getPushConstantRange(independentSets);

    if (!pushConstRange.size)
      return;

    m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      bindings->getPipelineLayout(independentSets),
      pushConstRange.stageFlags,
      pushConstRange.offset,
      pushConstRange.size,
      &m_state.pc.data[pushConstRange.offset]);
  }
  
  
  bool DxvkContext::commitComputeState() {
    this->spillRenderPass(false);

    if (m_flags.any(
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::CpDirtySpecConstants)) {
      if (unlikely(!this->updateComputePipelineState()))
        return false;
    }
    
    if (m_descriptorState.hasDirtyComputeSets())
      this->updateComputeShaderResources();

    if (m_flags.test(DxvkContextFlag::DirtyPushConstants))
      this->updatePushConstants<VK_PIPELINE_BIND_POINT_COMPUTE>();

    return true;
  }
  
  
  template<bool Indexed, bool Indirect>
  bool DxvkContext::commitGraphicsState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyPipeline)) {
      if (unlikely(!this->updateGraphicsPipeline()))
        return false;
    }
    
    if (m_flags.test(DxvkContextFlag::GpDirtyFramebuffer))
      this->updateFramebuffer();

    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound))
      this->startRenderPass();
    
    if (m_state.gp.flags.any(
          DxvkGraphicsPipelineFlag::HasStorageDescriptors,
          DxvkGraphicsPipelineFlag::HasTransformFeedback)) {
      this->commitGraphicsBarriers<Indexed, Indirect, false>();

      // This can only happen if the render pass was active before,
      // so we'll never strat the render pass twice in one draw
      if (!m_flags.test(DxvkContextFlag::GpRenderPassBound))
        this->startRenderPass();

      this->commitGraphicsBarriers<Indexed, Indirect, true>();
    }

    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer) && Indexed) {
      if (unlikely(!this->updateIndexBufferBinding()))
        return false;
    }
    
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers))
      this->updateVertexBufferBindings();
    
    if (m_flags.test(DxvkContextFlag::GpDirtySpecConstants))
      this->updateSpecConstants<VK_PIPELINE_BIND_POINT_GRAPHICS>();

    if (m_flags.test(DxvkContextFlag::GpDirtyPipelineState)) {
      DxvkGlobalPipelineBarrier barrier = { };

      if (Indexed) {
        barrier.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        barrier.access |= VK_ACCESS_INDEX_READ_BIT;
      }

      if (Indirect) {
        barrier.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        barrier.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
      }

      if (unlikely(!this->updateGraphicsPipelineState(barrier)))
        return false;
    }
    
    if (m_descriptorState.hasDirtyGraphicsSets())
      this->updateGraphicsShaderResources();
    
    if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback))
      this->updateTransformFeedbackState();
    
    this->updateDynamicState();
    
    if (m_flags.test(DxvkContextFlag::DirtyPushConstants))
      this->updatePushConstants<VK_PIPELINE_BIND_POINT_GRAPHICS>();

    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect)
      this->trackDrawBuffer();

    return true;
  }
  
  
  template<bool DoEmit>
  void DxvkContext::commitComputeBarriers() {
    const auto& layout = m_state.cp.pipeline->getBindings()->layout();

    // Exit early if we're only checking for hazards and
    // if the barrier set is empty, to avoid some overhead.
    if (!DoEmit && !m_execBarriers.hasResourceBarriers())
      return;

    for (uint32_t i = 0; i < DxvkDescriptorSets::CsSetCount; i++) {
      uint32_t bindingCount = layout.getBindingCount(i);

      for (uint32_t j = 0; j < bindingCount; j++) {
        const DxvkBindingInfo& binding = layout.getBinding(i, j);
        const DxvkShaderResourceSlot& slot = m_rc[binding.resourceBinding];

        bool requiresBarrier = false;

        switch (binding.descriptorType) {
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            if (likely(slot.bufferSlice.length())) {
              requiresBarrier = this->checkBufferBarrier<DoEmit>(slot.bufferSlice,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, binding.access);
            }
            break;

          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            if (likely(slot.bufferView != nullptr)) {
              requiresBarrier = this->checkBufferViewBarrier<DoEmit>(slot.bufferView,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, binding.access);
            }
            break;

          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if (likely(slot.imageView != nullptr)) {
              requiresBarrier = this->checkImageViewBarrier<DoEmit>(slot.imageView,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, binding.access);
            }
            break;

          default:
            /* nothing to do */;
        }

        if (requiresBarrier) {
          m_execBarriers.recordCommands(m_cmd);
          return;
        }
      }
    }
  }
  

  template<bool Indexed, bool Indirect, bool DoEmit>
  void DxvkContext::commitGraphicsBarriers() {
    if (m_barrierControl.test(DxvkBarrierControl::IgnoreGraphicsBarriers))
      return;

    constexpr auto storageBufferAccess = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
    constexpr auto storageImageAccess  = VK_ACCESS_SHADER_WRITE_BIT;

    bool requiresBarrier = false;

    // Check the draw buffer for indirect draw calls
    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect) {
      std::array<DxvkBufferSlice*, 2> slices = {{
        &m_state.id.argBuffer,
        &m_state.id.cntBuffer,
      }};

      for (uint32_t i = 0; i < slices.size() && !requiresBarrier; i++) {
        if ((slices[i]->length())
         && (slices[i]->buffer()->info().access & storageBufferAccess)) {
          requiresBarrier = this->checkBufferBarrier<DoEmit>(*slices[i],
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        }
      }
    }

    // Read-only stage, so we only have to check this if
    // the bindngs have actually changed between draws
    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer) && !requiresBarrier && Indexed) {
      const auto& indexBufferSlice = m_state.vi.indexBuffer;

      if ((indexBufferSlice.length())
       && (indexBufferSlice.bufferInfo().access & storageBufferAccess)) {
        requiresBarrier = this->checkBufferBarrier<DoEmit>(indexBufferSlice,
          VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
          VK_ACCESS_INDEX_READ_BIT);
      }
    }

    // Same here, also ignore unused vertex bindings
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers)) {
      uint32_t bindingCount = m_state.gp.state.il.bindingCount();

      for (uint32_t i = 0; i < bindingCount && !requiresBarrier; i++) {
        uint32_t binding = m_state.gp.state.ilBindings[i].binding();
        const auto& vertexBufferSlice = m_state.vi.vertexBuffers[binding];

        if ((vertexBufferSlice.length())
         && (vertexBufferSlice.bufferInfo().access & storageBufferAccess)) {
          requiresBarrier = this->checkBufferBarrier<DoEmit>(vertexBufferSlice,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        }
      }
    }

    // Transform feedback buffer writes won't overlap, so we
    // also only need to check those when they are rebound
    if (m_flags.test(DxvkContextFlag::GpDirtyXfbBuffers)
     && m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback)) {
      for (uint32_t i = 0; i < MaxNumXfbBuffers && !requiresBarrier; i++) {
        const auto& xfbBufferSlice = m_state.xfb.buffers[i];
        const auto& xfbCounterSlice = m_state.xfb.activeCounters[i];

        if (xfbBufferSlice.length()) {
          requiresBarrier = this->checkBufferBarrier<DoEmit>(xfbBufferSlice,
            VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
            VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT);

          if (xfbCounterSlice.length()) {
            requiresBarrier |= this->checkBufferBarrier<DoEmit>(xfbCounterSlice,
              VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
              VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
              VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
              VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT);
          }
        }
      }
    }

    // Check shader resources on every draw to handle WAW hazards
    auto layout = m_state.gp.pipeline->getBindings()->layout();

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount && !requiresBarrier; i++) {
      uint32_t bindingCount = layout.getBindingCount(i);

      for (uint32_t j = 0; j < bindingCount && !requiresBarrier; j++) {
        const DxvkBindingInfo& binding = layout.getBinding(i, j);
        const DxvkShaderResourceSlot& slot = m_rc[binding.resourceBinding];

        switch (binding.descriptorType) {
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            if ((slot.bufferSlice.length())
             && (slot.bufferSlice.bufferInfo().access & storageBufferAccess)) {
              requiresBarrier = this->checkBufferBarrier<DoEmit>(slot.bufferSlice,
                util::pipelineStages(binding.stage), binding.access);
            }
            break;

          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            if ((slot.bufferView != nullptr)
             && (slot.bufferView->buffer()->info().access & storageBufferAccess)) {
              requiresBarrier = this->checkBufferViewBarrier<DoEmit>(slot.bufferView,
                util::pipelineStages(binding.stage), binding.access);
            }
            break;

          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if ((slot.imageView != nullptr)
             && (slot.imageView->image()->info().access & storageImageAccess)) {
              requiresBarrier = this->checkImageViewBarrier<DoEmit>(slot.imageView,
                util::pipelineStages(binding.stage), binding.access);
            }
            break;

          default:
            /* nothing to do */;
        }
      }
    }

    // External subpass dependencies serve as full memory
    // and execution barriers, so we can use this to allow
    // inter-stage synchronization.
    if (requiresBarrier)
      this->spillRenderPass(true);
  }


  template<bool DoEmit>
  bool DxvkContext::checkBufferBarrier(
    const DxvkBufferSlice&          bufferSlice,
          VkPipelineStageFlags      stages,
          VkAccessFlags             access) {
    if constexpr (DoEmit) {
      m_execBarriers.accessBuffer(
        bufferSlice.getSliceHandle(),
        stages, access,
        bufferSlice.bufferInfo().stages,
        bufferSlice.bufferInfo().access);
      return false;
    } else {
      DxvkAccessFlags dstAccess = DxvkBarrierSet::getAccessTypes(access);

      bool dirty = m_execBarriers.isBufferDirty(
        bufferSlice.getSliceHandle(), dstAccess);

      if (!dirty || dstAccess.test(DxvkAccess::Read) || !this->canIgnoreWawHazards(stages))
        return dirty;

      DxvkAccessFlags srcAccess = m_execBarriers.getBufferAccess(bufferSlice.getSliceHandle());
      return srcAccess.test(DxvkAccess::Read);
    }
  }


  template<bool DoEmit>
  bool DxvkContext::checkBufferViewBarrier(
    const Rc<DxvkBufferView>&       bufferView,
          VkPipelineStageFlags      stages,
          VkAccessFlags             access) {
    if constexpr (DoEmit) {
      m_execBarriers.accessBuffer(
        bufferView->getSliceHandle(),
        stages, access,
        bufferView->buffer()->info().stages,
        bufferView->buffer()->info().access);
      return false;
    } else {
      DxvkAccessFlags dstAccess = DxvkBarrierSet::getAccessTypes(access);

      bool dirty = m_execBarriers.isBufferDirty(
        bufferView->getSliceHandle(), dstAccess);

      if (!dirty || dstAccess.test(DxvkAccess::Read) || !this->canIgnoreWawHazards(stages))
        return dirty;

      DxvkAccessFlags srcAccess = m_execBarriers.getBufferAccess(bufferView->getSliceHandle());
      return srcAccess.test(DxvkAccess::Read);
    }
  }


  template<bool DoEmit>
  bool DxvkContext::checkImageViewBarrier(
    const Rc<DxvkImageView>&        imageView,
          VkPipelineStageFlags      stages,
          VkAccessFlags             access) {
    if constexpr (DoEmit) {
      m_execBarriers.accessImage(
        imageView->image(),
        imageView->imageSubresources(),
        imageView->image()->info().layout,
        stages, access,
        imageView->image()->info().layout,
        imageView->image()->info().stages,
        imageView->image()->info().access);
      return false;
    } else {
      DxvkAccessFlags dstAccess = DxvkBarrierSet::getAccessTypes(access);

      bool dirty = m_execBarriers.isImageDirty(
        imageView->image(),
        imageView->imageSubresources(),
        dstAccess);

      if (!dirty || dstAccess.test(DxvkAccess::Read) || !this->canIgnoreWawHazards(stages))
        return dirty;

      DxvkAccessFlags srcAccess = m_execBarriers.getImageAccess(
        imageView->image(), imageView->imageSubresources());
      return srcAccess.test(DxvkAccess::Read);
    }
  }


  bool DxvkContext::canIgnoreWawHazards(VkPipelineStageFlags stages) {
    if (!m_barrierControl.test(DxvkBarrierControl::IgnoreWriteAfterWrite))
      return false;

    if (stages & VK_SHADER_STAGE_COMPUTE_BIT) {
      VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      return !(m_execBarriers.getSrcStages() & ~stageMask);
    }

    return true;
  }


  void DxvkContext::emitMemoryBarrier(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
    m_cmd->addStatCtr(DxvkStatCounter::CmdBarrierCount, 1);
  }


  void DxvkContext::trackDrawBuffer() {
    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer)) {
      m_flags.clr(DxvkContextFlag::DirtyDrawBuffer);

      if (m_state.id.argBuffer.length())
        m_cmd->trackResource<DxvkAccess::Read>(m_state.id.argBuffer.buffer());

      if (m_state.id.cntBuffer.length())
        m_cmd->trackResource<DxvkAccess::Read>(m_state.id.cntBuffer.buffer());
    }
  }


  bool DxvkContext::tryInvalidateDeviceLocalBuffer(
      const Rc<DxvkBuffer>&           buffer,
            VkDeviceSize              copySize) {
    // We can only discard if the full buffer gets written, and we will only discard
    // small buffers in order to not waste significant amounts of memory.
    if (copySize != buffer->info().size || copySize > 0x40000)
      return false;

    // Check if the buffer is safe to move at all
    if (!buffer->canRelocate())
      return false;

    // Suspend the current render pass if transform feedback is active prior to
    // invalidating the buffer, since otherwise we may invalidate a bound buffer.
    if ((buffer->info().usage & VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT)
     && (m_flags.test(DxvkContextFlag::GpXfbActive)))
      this->spillRenderPass(true);

    this->invalidateBuffer(buffer, buffer->allocateSlice());
    return true;
  }


  Rc<DxvkImageView> DxvkContext::ensureImageViewCompatibility(
    const Rc<DxvkImageView>&        view,
          VkImageUsageFlagBits      usage) {
    // Return existing view if it already compatible with the image
    VkFormat viewFormat = view->info().format;

    bool isFormatCompatible = view->image()->isViewCompatible(viewFormat);
    bool isUsageCompatible = (view->image()->info().usage & usage) == usage;

    if (isFormatCompatible && isUsageCompatible) {
      if (view->info().usage & usage)
        return view;

      // Just create a new view with the correct usage flag
      DxvkImageViewKey viewInfo = view->info();
      viewInfo.usage = usage;
      return view->image()->createView(viewInfo);
    } else {
      // Actually need to relocate the image
      DxvkImageUsageInfo usageInfo = { };
      usageInfo.usage = usage;

      if (!isFormatCompatible) {
        usageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        usageInfo.viewFormatCount = 1u;
        usageInfo.viewFormats = &viewFormat;
      }

      if (!ensureImageCompatibility(view->image(), usageInfo))
        return nullptr;

      DxvkImageViewKey viewInfo = view->info();
      viewInfo.usage = usage;
      return view->image()->createView(viewInfo);
    }
  }


  void DxvkContext::relocateResources(
          size_t                    bufferCount,
    const DxvkRelocateBufferInfo*   bufferInfos,
          size_t                    imageCount,
    const DxvkRelocateImageInfo*    imageInfos) {
    if (!bufferCount && !imageCount)
      return;

    // Ensure images are in the expected layout and any sort of layout
    // tracking does not happen after the backing storage is swapped.
    for (size_t i = 0; i < imageCount; i++)
      prepareImage(imageInfos[i].image, imageInfos[i].image->getAvailableSubresources());

    m_execBarriers.recordCommands(m_cmd);

    small_vector<VkImageMemoryBarrier2, 16> imageBarriers;

    VkMemoryBarrier2 bufferBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

    for (size_t i = 0; i < bufferCount; i++) {
      const auto& info = bufferInfos[i];

      bufferBarrier.srcStageMask |= info.buffer->info().stages;
      bufferBarrier.srcAccessMask |= info.buffer->info().access;
    }

    for (size_t i = 0; i < imageCount; i++) {
      const auto& info = imageInfos[i];
      auto oldStorage = info.image->getAllocation();

      VkImageMemoryBarrier2 dstBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      dstBarrier.srcStageMask = info.image->info().stages;
      dstBarrier.srcAccessMask = info.image->info().access;
      dstBarrier.dstStageMask = info.image->info().stages;
      dstBarrier.dstAccessMask = info.image->info().access;
      dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      dstBarrier.newLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.image = info.storage->getImageInfo().image;
      dstBarrier.subresourceRange = info.image->getAvailableSubresources();

      VkImageMemoryBarrier2 srcBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      srcBarrier.srcStageMask = info.image->info().stages;
      srcBarrier.srcAccessMask = info.image->info().access;
      srcBarrier.dstStageMask = info.image->info().stages;
      srcBarrier.dstAccessMask = info.image->info().access;
      srcBarrier.oldLayout = info.image->info().layout;
      srcBarrier.newLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      srcBarrier.image = oldStorage->getImageInfo().image;
      srcBarrier.subresourceRange = info.image->getAvailableSubresources();

      imageBarriers.push_back(dstBarrier);
      imageBarriers.push_back(srcBarrier);
    }

    // Submit all pending barriers in one go
    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    if (imageCount) {
      depInfo.imageMemoryBarrierCount = imageBarriers.size();
      depInfo.pImageMemoryBarriers = imageBarriers.data();
    }

    if (bufferCount) {
      depInfo.memoryBarrierCount = 1u;
      depInfo.pMemoryBarriers = &bufferBarrier;
    }

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    imageBarriers.clear();

    // Copy and invalidate all buffers
    for (size_t i = 0; i < bufferCount; i++) {
      const auto& info = bufferInfos[i];
      auto oldStorage = info.buffer->getAllocation();

      DxvkResourceBufferInfo dstInfo = info.storage->getBufferInfo();
      DxvkResourceBufferInfo srcInfo = oldStorage->getBufferInfo();

      VkBufferCopy2 region = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
      region.dstOffset = dstInfo.offset;
      region.srcOffset = srcInfo.offset;
      region.size = info.buffer->info().size;

      VkCopyBufferInfo2 copy = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
      copy.dstBuffer = dstInfo.buffer;
      copy.srcBuffer = srcInfo.buffer;
      copy.regionCount = 1;
      copy.pRegions = &region;

      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &copy);
      m_cmd->trackResource<DxvkAccess::Write>(info.buffer);

      invalidateBuffer(info.buffer, Rc<DxvkResourceAllocation>(info.storage));
    }

    // Copy and invalidate all images
    for (size_t i = 0; i < imageCount; i++) {
      const auto& info = imageInfos[i];
      auto oldStorage = info.image->getAllocation();

      DxvkResourceImageInfo dstInfo = info.storage->getImageInfo();
      DxvkResourceImageInfo srcInfo = oldStorage->getImageInfo();

      // Iterate over all subresources and compute copy regions. We need
      // one region per mip or plane, so size the local array accordingly.
      small_vector<VkImageCopy2, 16> imageRegions;

      uint32_t planeCount = 1;

      auto formatInfo = info.image->formatInfo();

      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane))
        planeCount = vk::getPlaneCount(formatInfo->aspectMask);

      for (uint32_t p = 0; p < planeCount; p++) {
        for (uint32_t m = 0; m < info.image->info().mipLevels; m++) {
          VkImageCopy2 region = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
          region.dstSubresource.aspectMask = formatInfo->aspectMask;
          region.dstSubresource.mipLevel = m;
          region.dstSubresource.baseArrayLayer = 0;
          region.dstSubresource.layerCount = info.image->info().numLayers;
          region.srcSubresource = region.dstSubresource;
          region.extent = info.image->mipLevelExtent(m);

          if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
            region.dstSubresource.aspectMask = vk::getPlaneAspect(p);
            region.srcSubresource.aspectMask = vk::getPlaneAspect(p);

            region.extent.width /= formatInfo->planes[p].blockSize.width;
            region.extent.height /= formatInfo->planes[p].blockSize.height;
          }

          imageRegions.push_back(region);
        }
      }

      VkCopyImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
      copy.dstImage = dstInfo.image;
      copy.dstImageLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      copy.srcImage = srcInfo.image;
      copy.srcImageLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      copy.regionCount = imageRegions.size();
      copy.pRegions = imageRegions.data();

      m_cmd->cmdCopyImage(DxvkCmdBuffer::ExecBuffer, &copy);
      m_cmd->trackResource<DxvkAccess::Write>(info.image);

      // Invalidate image and emit post-copy barrier to use the correct layout
      invalidateImageWithUsage(info.image, Rc<DxvkResourceAllocation>(info.storage), info.usageInfo);

      VkImageMemoryBarrier2 dstBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      dstBarrier.srcStageMask = info.image->info().stages;
      dstBarrier.srcAccessMask = info.image->info().access;
      dstBarrier.dstStageMask = info.image->info().stages;
      dstBarrier.dstAccessMask = info.image->info().access;
      dstBarrier.oldLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      dstBarrier.newLayout = info.image->info().layout;
      dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.image = info.storage->getImageInfo().image;
      dstBarrier.subresourceRange = info.image->getAvailableSubresources();

      imageBarriers.push_back(dstBarrier);
    }

    // Submit post-copy barriers
    bufferBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    bufferBarrier.srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;

    for (size_t i = 0; i < bufferCount; i++) {
      const auto& info = bufferInfos[i];

      bufferBarrier.dstStageMask |= info.buffer->info().stages;
      bufferBarrier.dstAccessMask |= info.buffer->info().access;
    }

    if (imageCount) {
      depInfo.imageMemoryBarrierCount = imageBarriers.size();
      depInfo.pImageMemoryBarriers = imageBarriers.data();
    }

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
  }


  Rc<DxvkSampler> DxvkContext::createBlitSampler(
          VkFilter                    filter) {
    DxvkSamplerKey samplerKey;
    samplerKey.setFilter(filter, filter,
      VK_SAMPLER_MIPMAP_MODE_NEAREST);
    samplerKey.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    return m_device->createSampler(samplerKey);
  }


  DxvkGraphicsPipeline* DxvkContext::lookupGraphicsPipeline(
    const DxvkGraphicsPipelineShaders&  shaders) {
    auto idx = shaders.hash() % m_gpLookupCache.size();
    
    if (unlikely(!m_gpLookupCache[idx] || !shaders.eq(m_gpLookupCache[idx]->shaders())))
      m_gpLookupCache[idx] = m_common->pipelineManager().createGraphicsPipeline(shaders);

    return m_gpLookupCache[idx];
  }


  DxvkComputePipeline* DxvkContext::lookupComputePipeline(
    const DxvkComputePipelineShaders&   shaders) {
    auto idx = shaders.hash() % m_cpLookupCache.size();
    
    if (unlikely(!m_cpLookupCache[idx] || !shaders.eq(m_cpLookupCache[idx]->shaders())))
      m_cpLookupCache[idx] = m_common->pipelineManager().createComputePipeline(shaders);

    return m_cpLookupCache[idx];
  }


  Rc<DxvkBuffer> DxvkContext::createZeroBuffer(
          VkDeviceSize              size) {
    if (m_zeroBuffer != nullptr && m_zeroBuffer->info().size >= size)
      return m_zeroBuffer;

    DxvkBufferCreateInfo bufInfo;
    bufInfo.size    = align<VkDeviceSize>(size, 1 << 20);
    bufInfo.usage   = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.stages  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufInfo.access  = VK_ACCESS_TRANSFER_WRITE_BIT
                    | VK_ACCESS_TRANSFER_READ_BIT;

    m_zeroBuffer = m_device->createBuffer(bufInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkBufferSliceHandle slice = m_zeroBuffer->getSliceHandle();

    m_cmd->cmdFillBuffer(DxvkCmdBuffer::InitBuffer,
      slice.handle, slice.offset, slice.length, 0);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);
    m_cmd->addStatCtr(DxvkStatCounter::CmdBarrierCount, 1);

    return m_zeroBuffer;
  }
  

  void DxvkContext::resizeDescriptorArrays(
          uint32_t                  bindingCount) {
    m_descriptors.resize(bindingCount);
    m_descriptorWrites.resize(bindingCount);

    for (uint32_t i = 0; i < bindingCount; i++) {
      m_descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      m_descriptorWrites[i].descriptorCount = 1;
      m_descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
      m_descriptorWrites[i].pImageInfo = &m_descriptors[i].image;
      m_descriptorWrites[i].pBufferInfo = &m_descriptors[i].buffer;
      m_descriptorWrites[i].pTexelBufferView = &m_descriptors[i].texelBuffer;
    }
  }


  void DxvkContext::beginCurrentCommands() {
    // Mark all resources as untracked
    m_vbTracked.clear();
    m_rcTracked.clear();

    // The current state of the internal command buffer is
    // undefined, so we have to bind and set up everything
    // before any draw or dispatch command is recorded.
    m_flags.clr(
      DxvkContextFlag::GpRenderPassBound,
      DxvkContextFlag::GpXfbActive,
      DxvkContextFlag::GpIndependentSets);

    m_flags.set(
      DxvkContextFlag::GpDirtyFramebuffer,
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::GpDirtyXfbBuffers,
      DxvkContextFlag::GpDirtyBlendConstants,
      DxvkContextFlag::GpDirtyStencilRef,
      DxvkContextFlag::GpDirtyMultisampleState,
      DxvkContextFlag::GpDirtyRasterizerState,
      DxvkContextFlag::GpDirtyViewport,
      DxvkContextFlag::GpDirtyDepthBias,
      DxvkContextFlag::GpDirtyDepthBounds,
      DxvkContextFlag::GpDirtyDepthStencilState,
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::DirtyDrawBuffer);

    m_descriptorState.dirtyStages(
      VK_SHADER_STAGE_ALL_GRAPHICS |
      VK_SHADER_STAGE_COMPUTE_BIT);

    m_state.gp.pipeline = nullptr;
    m_state.cp.pipeline = nullptr;
  }


  void DxvkContext::endCurrentCommands() {
    this->spillRenderPass(true);
    this->flushSharedImages();

    m_sdmaBarriers.finalize(m_cmd);
    m_initBarriers.finalize(m_cmd);
    m_execBarriers.finalize(m_cmd);
  }


  void DxvkContext::splitCommands() {
    // This behaves the same as a pair of endRecording and
    // beginRecording calls, except that we keep the same
    // command list object for subsequent commands.
    this->endCurrentCommands();

    m_cmd->next();

    this->beginCurrentCommands();
  }


  bool DxvkContext::formatsAreCopyCompatible(
          VkFormat                  imageFormat,
          VkFormat                  bufferFormat) {
    if (!bufferFormat)
      bufferFormat = imageFormat;

    // Depth-stencil data is packed differently in client APIs than what
    // we can do in Vulkan, and these formats cannot be reinterpreted.
    auto imageFormatInfo = lookupFormatInfo(imageFormat);
    auto bufferFormatInfo = lookupFormatInfo(bufferFormat);

    return !((imageFormatInfo->aspectMask | bufferFormatInfo->aspectMask) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
  }


  VkFormat DxvkContext::sanitizeTexelBufferFormat(
          VkFormat                  srcFormat) {
    switch (srcFormat) {
      case VK_FORMAT_S8_UINT:
        return VK_FORMAT_R8_UINT;

      case VK_FORMAT_D16_UNORM:
        return VK_FORMAT_R16_UINT;

      case VK_FORMAT_D16_UNORM_S8_UINT:
        return VK_FORMAT_R16G16_UINT;

      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
        return VK_FORMAT_R32_UINT;

      case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_FORMAT_R32G32_UINT;

      default:
        return VK_FORMAT_UNDEFINED;
    }
  }

}
