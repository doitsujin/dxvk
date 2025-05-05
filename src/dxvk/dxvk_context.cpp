#include <cstring>
#include <map>
#include <vector>
#include <utility>

#include "dxvk_device.h"
#include "dxvk_context.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<DxvkDevice>& device)
  : m_device      (device),
    m_common      (&device->m_objects),
    m_sdmaAcquires(DxvkCmdBuffer::SdmaBarriers),
    m_sdmaBarriers(DxvkCmdBuffer::SdmaBuffer),
    m_initAcquires(DxvkCmdBuffer::InitBarriers),
    m_initBarriers(DxvkCmdBuffer::InitBuffer),
    m_execBarriers(DxvkCmdBuffer::ExecBuffer),
    m_queryManager(m_common->queryPool()),
    m_implicitResolves(device) {
    // Init framebuffer info with default render pass in case
    // the app does not explicitly bind any render targets
    m_state.om.framebufferInfo = makeFramebufferInfo(m_state.om.renderTargets);
    m_descriptorManager = new DxvkDescriptorManager(device.ptr());

    // Global barrier for graphics pipelines. This is only used to
    // avoid write-after-read hazards after a render pass, so the
    // access mask here can be zero.
    m_renderPassBarrierDst.stages = m_device->getShaderPipelineStages()
                                  | VK_PIPELINE_STAGE_TRANSFER_BIT
                                  | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    if (m_device->features().extTransformFeedback.transformFeedback)
      m_renderPassBarrierDst.stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

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

    // Check whether we can batch direct draws
    if (m_device->features().extMultiDraw.multiDraw
     && m_device->properties().extMultiDraw.maxMultiDrawCount >= DirectMultiDrawBatchSize)
      m_features.set(DxvkContextFeature::DirectMultiDraw);

    // Add a fast path to query debug utils support
    if (m_device->debugFlags().test(DxvkDebugFlag::Capture))
      m_features.set(DxvkContextFeature::DebugUtils);
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
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording(
    const VkDebugUtilsLabelEXT*       reason) {
    this->endCurrentCommands();
    this->relocateQueuedResources();

    m_implicitResolves.cleanup(m_trackingId);

    if (m_descriptorPool->shouldSubmit(false)) {
      m_cmd->trackDescriptorPool(m_descriptorPool, m_descriptorManager);
      m_descriptorPool = m_descriptorManager->getDescriptorPool();
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      // Make sure to emit the submission reason always at the very end
      if (reason && reason->pLabelName && reason->pLabelName[0])
        m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, *reason);
    }

    m_cmd->finalize();
    return std::exchange(m_cmd, nullptr);
  }


  void DxvkContext::endFrame() {
    if (m_descriptorPool->shouldSubmit(true)) {
      m_cmd->trackDescriptorPool(m_descriptorPool, m_descriptorManager);
      m_descriptorPool = m_descriptorManager->getDescriptorPool();
    }

    m_renderPassIndex = 0u;
  }


  void DxvkContext::beginLatencyTracking(
    const Rc<DxvkLatencyTracker>&     tracker,
          uint64_t                    frameId) {
    if (tracker && (!m_latencyTracker || m_latencyTracker == tracker)) {
      tracker->notifyCsRenderBegin(frameId);

      m_latencyTracker = tracker;
      m_latencyFrameId = frameId;

      m_endLatencyTracking = false;
    }
  }


  void DxvkContext::endLatencyTracking(
    const Rc<DxvkLatencyTracker>&     tracker) {
    if (tracker && tracker == m_latencyTracker)
      m_endLatencyTracking = true;
  }


  void DxvkContext::flushCommandList(
    const VkDebugUtilsLabelEXT*       reason,
          DxvkSubmitStatus*           status) {
    // Need to call this before submitting so that the last GPU
    // submission does not happen before the render end signal.
    if (m_endLatencyTracking && m_latencyTracker)
      m_latencyTracker->notifyCsRenderEnd(m_latencyFrameId);

    m_device->submitCommandList(this->endRecording(reason),
      m_latencyTracker, m_latencyFrameId, status);

    // Ensure that subsequent submissions do not see the tracker.
    // It is important to hide certain internal submissions in
    // case the application is CPU-bound.
    if (m_endLatencyTracking) {
      m_latencyTracker = nullptr;
      m_latencyFrameId = 0u;

      m_endLatencyTracking = false;
    }

    // If we have a zero buffer, see if we can get rid of it
    freeZeroBuffer();

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

  
  void DxvkContext::beginQuery(const Rc<DxvkQuery>& query) {
    m_queryManager.enableQuery(m_cmd, query);
  }


  void DxvkContext::endQuery(const Rc<DxvkQuery>& query) {
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

      flushPendingAccesses(*image, subresources, DxvkAccess::Write);

      accessImage(DxvkCmdBuffer::ExecBuffer, *image, subresources,
        image->info().layout, image->info().stages, 0,
        layout, image->info().stages, image->info().access, DxvkAccessOp::None);

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

      m_cmd->track(image, DxvkAccess::Write);
    }
  }


  void DxvkContext::clearBuffer(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          uint32_t              value) {
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    if (!prepareOutOfOrderTransfer(buffer, offset, length, DxvkAccess::Write)) {
      spillRenderPass(true);

      flushPendingAccesses(*buffer, offset, length, DxvkAccess::Write);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    auto bufferSlice = buffer->getSliceHandle(offset, align(length, sizeof(uint32_t)));

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

    accessBuffer(cmdBuffer, *buffer, offset, length,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      DxvkAccessOp::None);

    m_cmd->track(buffer, DxvkAccess::Write);
  }
  
  
  void DxvkContext::clearBufferView(
    const Rc<DxvkBufferView>&   bufferView,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          VkClearColorValue     value) {
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    if (!prepareOutOfOrderTransfer(bufferView, offset, length, DxvkAccess::Write)) {
      spillRenderPass(true);
      invalidateState();

      flushPendingAccesses(*bufferView, DxvkAccess::Write);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = bufferView->buffer()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(cmdBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Clear view (",
          dstName ? dstName : "unknown", ")").c_str()));
    }

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

    m_cmd->cmdBindPipeline(cmdBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(cmdBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    m_cmd->cmdPushConstants(cmdBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(cmdBuffer,
      workgroups.width, workgroups.height, workgroups.depth);

    accessBuffer(cmdBuffer, *bufferView,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_WRITE_BIT, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(cmdBuffer);

    m_cmd->track(bufferView->buffer(), DxvkAccess::Write);
  }
  
  
  void DxvkContext::clearRenderTarget(
    const Rc<DxvkImageView>&    imageView,
          VkImageAspectFlags    clearAspects,
          VkClearValue          clearValue,
          VkImageAspectFlags    discardAspects) {
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
      this->prepareImage(imageView->image(), imageView->imageSubresources(), false);
    } else if (!m_state.om.framebufferInfo.isWritable(attachmentIndex, clearAspects)) {
      // We cannot inline clears if the clear aspects are not writable. End the
      // render pass on the next draw to ensure that the image gets cleared.
      if (m_flags.test(DxvkContextFlag::GpRenderPassBound))
        m_flags.set(DxvkContextFlag::GpRenderPassNeedsFlush);
    }

    // Unconditionally defer clears until either the next render pass, or the
    // next draw if there is no reason to interrupt the render pass. This is
    // useful to adjust store ops for tilers, and ensures that pending resolves
    // are handled correctly.
    if (discardAspects)
      this->deferDiscard(imageView, discardAspects);

    if (clearAspects)
      this->deferClear(imageView, clearAspects, clearValue);

    // Invalidate implicit resolves
    if (imageView->isMultisampled()) {
      auto subresources = imageView->imageSubresources();
      subresources.aspectMask = clearAspects;

      m_implicitResolves.invalidate(*imageView->image(), subresources);
    }
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

    if (imageView->isMultisampled()) {
      auto subresources = imageView->imageSubresources();
      subresources.aspectMask = aspect;

      m_implicitResolves.invalidate(*imageView->image(), subresources);
    }
  }
  
  
  void DxvkContext::copyBuffer(
    const Rc<DxvkBuffer>&       dstBuffer,
          VkDeviceSize          dstOffset,
    const Rc<DxvkBuffer>&       srcBuffer,
          VkDeviceSize          srcOffset,
          VkDeviceSize          numBytes) {
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    if (!prepareOutOfOrderTransfer(srcBuffer, srcOffset, numBytes, DxvkAccess::Read)
     || !prepareOutOfOrderTransfer(dstBuffer, dstOffset, numBytes, DxvkAccess::Write)) {
      this->spillRenderPass(true);

      flushPendingAccesses(*srcBuffer, srcOffset, numBytes, DxvkAccess::Read);
      flushPendingAccesses(*dstBuffer, dstOffset, numBytes, DxvkAccess::Write);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    auto srcSlice = srcBuffer->getSliceHandle(srcOffset, numBytes);
    auto dstSlice = dstBuffer->getSliceHandle(dstOffset, numBytes);

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

    accessBuffer(cmdBuffer, *srcBuffer, srcOffset, numBytes,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
      DxvkAccessOp::None);

    accessBuffer(cmdBuffer, *dstBuffer, dstOffset, numBytes,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      DxvkAccessOp::None);

    m_cmd->track(dstBuffer, DxvkAccess::Write);
    m_cmd->track(srcBuffer, DxvkAccess::Read);
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
      bufInfo.debugName = "Temp buffer";

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
            && (srcImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT)
            && (srcImage != dstImage);
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

    if (dstImage->info().sampleCount > VK_SAMPLE_COUNT_1_BIT)
      m_implicitResolves.invalidate(*dstImage, vk::makeSubresourceRange(dstSubresource));
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
      imgInfo.debugName     = "Temp image";

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
      copyImageToBufferCs(dstBuffer, dstOffset, rowAlignment, sliceAlignment,
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
    viewInfo.size = elementSize * util::flattenImageExtent(dstSize);
    viewInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    Rc<DxvkBufferView> dstView = dstBuffer->createView(viewInfo);

    viewInfo.offset = srcBufferOffset;
    viewInfo.size = elementSize * util::flattenImageExtent(srcSize);
    viewInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    Rc<DxvkBufferView> srcView = srcBuffer->createView(viewInfo);

    flushPendingAccesses(*dstView, DxvkAccess::Write);
    flushPendingAccesses(*srcView, DxvkAccess::Read);

    if (srcBuffer == dstBuffer
     && srcView->info().offset < dstView->info().offset + dstView->info().size
     && srcView->info().offset + srcView->info().size > dstView->info().offset) {
      // Create temporary copy in case of overlapping regions
      DxvkBufferCreateInfo bufferInfo;
      bufferInfo.size   = srcView->info().size;
      bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                        | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                        | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      bufferInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
                        | VK_ACCESS_SHADER_READ_BIT;
      bufferInfo.debugName = "Temp buffer";

      Rc<DxvkBuffer> tmpBuffer = m_device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      auto tmpBufferSlice = tmpBuffer->getSliceHandle();
      auto srcBufferSlice = srcView->getSliceHandle();

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

      m_cmd->track(tmpBuffer, DxvkAccess::Write);
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
    
    accessBuffer(DxvkCmdBuffer::ExecBuffer, *dstView,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
      DxvkAccessOp::None);

    accessBuffer(DxvkCmdBuffer::ExecBuffer, *srcView,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      DxvkAccessOp::None);

    // Track all involved resources
    m_cmd->track(dstBuffer, DxvkAccess::Write);
    m_cmd->track(srcBuffer, DxvkAccess::Read);
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


  void DxvkContext::discardImage(
    const Rc<DxvkImage>&          image) {
    VkImageUsageFlags imageUsage = image->info().usage;

    if (!(imageUsage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)))
      return;

    // Perform store op optimization on bound render targets
    VkImageSubresourceRange subresource = { };
    subresource.aspectMask = image->formatInfo()->aspectMask;
    subresource.layerCount = image->info().numLayers;
    subresource.levelCount = image->info().mipLevels;

    discardRenderTarget(*image, subresource);

    // We don't really have a good way to queue up discards for
    // subsequent render passes here without a view, so don't.
  }


  void DxvkContext::dispatch(
          uint32_t x,
          uint32_t y,
          uint32_t z) {
    if (this->commitComputeState()) {
      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
      
      m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer, x, y, z);
      m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1u);

      m_queryManager.endQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);
    }
  }
  
  
  void DxvkContext::dispatchIndirect(
          VkDeviceSize      offset) {
    auto bufferSlice = m_state.id.argBuffer.getSliceHandle(
      offset, sizeof(VkDispatchIndirectCommand));

    flushPendingAccesses(
      *m_state.id.argBuffer.buffer(),
      m_state.id.argBuffer.offset() + offset,
      sizeof(VkDispatchIndirectCommand), DxvkAccess::Read);

    if (this->commitComputeState()) {
      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);

      m_cmd->cmdDispatchIndirect(DxvkCmdBuffer::ExecBuffer,
        bufferSlice.handle, bufferSlice.offset);
      m_cmd->addStatCtr(DxvkStatCounter::CmdDispatchCalls, 1u);

      m_queryManager.endQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);

      accessDrawBuffer(offset, 1, 0, sizeof(VkDispatchIndirectCommand));

      this->trackDrawBuffer();
    }
  }
  
  
  void DxvkContext::draw(
          uint32_t          count,
    const VkDrawIndirectCommand* draws) {
    drawGeneric<false>(count, draws);
  }
  
  
  void DxvkContext::drawIndirect(
          VkDeviceSize      offset,
          uint32_t          count,
          uint32_t          stride,
          bool              unroll) {
    drawIndirectGeneric<false>(offset, count, stride, unroll);
  }
  
  
  void DxvkContext::drawIndirectCount(
          VkDeviceSize      offset,
          VkDeviceSize      countOffset,
          uint32_t          maxCount,
          uint32_t          stride) {
    drawIndirectCountGeneric<false>(offset, countOffset, maxCount, stride);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t          count,
    const VkDrawIndexedIndirectCommand* draws) {
    drawGeneric<true>(count, draws);
  }


  void DxvkContext::drawIndexedIndirect(
          VkDeviceSize      offset,
          uint32_t          count,
          uint32_t          stride,
          bool              unroll) {
    drawIndirectGeneric<true>(offset, count, stride, unroll);
  }
  
  
  void DxvkContext::drawIndexedIndirectCount(
          VkDeviceSize      offset,
          VkDeviceSize      countOffset,
          uint32_t          maxCount,
          uint32_t          stride) {
    drawIndirectCountGeneric<true>(offset, countOffset, maxCount, stride);
  }


  void DxvkContext::drawIndirectXfb(
          VkDeviceSize      counterOffset,
          uint32_t          counterDivisor,
          uint32_t          counterBias) {
    if (this->commitGraphicsState<false, true>()) {
      auto physSlice = m_state.id.cntBuffer.getSliceHandle();

      m_cmd->cmdDrawIndirectVertexCount(1, 0,
        physSlice.handle, physSlice.offset + counterOffset,
        counterBias, counterDivisor);
      m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

      // The count will generally be written from streamout
      if (likely(m_state.id.cntBuffer.buffer()->hasGfxStores()))
        accessDrawCountBuffer(counterOffset);
    }
  }


  void DxvkContext::initBuffer(
    const Rc<DxvkBuffer>&           buffer) {
    auto dstSlice = buffer->getSliceHandle();

    // If the buffer is suballocated, clear the entire allocated
    // region, which is guaranteed to have a nicely aligned size
    if (!buffer->storage()->flags().test(DxvkAllocationFlag::OwnsBuffer)) {
      auto bufferInfo = buffer->storage()->getBufferInfo();

      dstSlice.handle = bufferInfo.buffer;
      dstSlice.offset = bufferInfo.offset;
      dstSlice.length = bufferInfo.size;
    }

    // Buffer size may be misaligned, in which case we have
    // to use a plain buffer copy to fill the last few bytes.
    constexpr VkDeviceSize MinCopyAndFillSize = 1u << 20;

    VkDeviceSize copySize = dstSlice.length & 3u;
    VkDeviceSize fillSize = dstSlice.length - copySize;

    // If the buffer is small, just dispatch one single copy
    if (copySize && dstSlice.length < MinCopyAndFillSize) {
      copySize = dstSlice.length;
      fillSize = 0u;
    }

    if (fillSize) {
      m_cmd->cmdFillBuffer(DxvkCmdBuffer::SdmaBuffer,
        dstSlice.handle, dstSlice.offset, fillSize, 0u);
    }

    if (copySize) {
      auto srcSlice = createZeroBuffer(copySize)->getSliceHandle();

      VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
      copyRegion.srcOffset = srcSlice.offset;
      copyRegion.dstOffset = dstSlice.offset + fillSize;
      copyRegion.size      = copySize;

      VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
      copyInfo.srcBuffer = srcSlice.handle;
      copyInfo.dstBuffer = dstSlice.handle;
      copyInfo.regionCount = 1;
      copyInfo.pRegions = &copyRegion;

      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::SdmaBuffer, &copyInfo);
    }

    accessBufferTransfer(*buffer,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    m_cmd->track(buffer, DxvkAccess::Write);
  }


  void DxvkContext::initImage(
    const Rc<DxvkImage>&            image,
          VkImageLayout             initialLayout) {
    VkImageSubresourceRange subresources = image->getAvailableSubresources();

    if (initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
      accessImage(DxvkCmdBuffer::InitBarriers,
        *image, subresources, initialLayout,
        VK_PIPELINE_STAGE_2_NONE, 0, DxvkAccessOp::None);

      m_cmd->track(image, DxvkAccess::None);
    } else {
      auto formatInfo = image->formatInfo();
      auto bufferInfo = image->storage()->getBufferInfo();

      if (!formatInfo->flags.any(DxvkFormatFlag::BlockCompressed, DxvkFormatFlag::MultiPlane)) {
        // Clear commands can only happen on the graphics queue
        VkImageLayout clearLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        addImageInitTransition(*image, subresources, clearLayout,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        flushImageLayoutTransitions(DxvkCmdBuffer::InitBuffer);

        if (subresources.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
          VkClearDepthStencilValue value = { };

          m_cmd->cmdClearDepthStencilImage(DxvkCmdBuffer::InitBuffer,
            image->handle(), clearLayout, &value, 1, &subresources);
        } else {
          VkClearColorValue value = { };

          m_cmd->cmdClearColorImage(DxvkCmdBuffer::InitBuffer,
            image->handle(), clearLayout, &value, 1, &subresources);
        }

        accessImage(DxvkCmdBuffer::InitBuffer, *image, subresources, clearLayout,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);
      } else if (bufferInfo.buffer) {
        // If the image allocation has a buffer, use that to clear the backing storage
        // to zero. There is no strict guarantee that the image contents read zero, but
        // in practice this should be good enough and avoids having to create a very
        // large zero buffer in some cases.
        m_cmd->cmdFillBuffer(DxvkCmdBuffer::SdmaBuffer,
          bufferInfo.buffer, bufferInfo.offset, bufferInfo.size, 0u);

        accessMemory(DxvkCmdBuffer::SdmaBuffer,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_NONE);

        accessImage(DxvkCmdBuffer::InitBarriers, *image, subresources, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_NONE, DxvkAccessOp::None);
      } else {
        // We're copying from a buffer, use the transfer queue
        addImageInitTransition(*image, subresources, VK_IMAGE_LAYOUT_GENERAL,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        flushImageLayoutTransitions(DxvkCmdBuffer::SdmaBuffer);

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

          auto zeroSlice = createZeroBuffer(dataSize)->getSliceHandle();

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
              copyRegion.bufferOffset = zeroSlice.offset;
              copyRegion.imageSubresource = vk::makeSubresourceLayers(
                vk::pickSubresource(subresources, level, layer));
              copyRegion.imageSubresource.aspectMask = aspect;
              copyRegion.imageOffset = offset;
              copyRegion.imageExtent = extent;

              VkCopyBufferToImageInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
              copyInfo.srcBuffer = zeroSlice.handle;
              copyInfo.dstImage = image->handle();
              copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
              copyInfo.regionCount = 1;
              copyInfo.pRegions = &copyRegion;

              m_cmd->cmdCopyBufferToImage(DxvkCmdBuffer::SdmaBuffer, &copyInfo);
            }
          }
        }

        accessImageTransfer(*image, subresources, VK_IMAGE_LAYOUT_GENERAL,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      }

      m_cmd->track(image, DxvkAccess::Write);
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
    accessImage(DxvkCmdBuffer::InitBuffer, *image, image->getAvailableSubresources(),
      VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_NONE, 0, DxvkAccessOp::None);

    m_cmd->track(image, DxvkAccess::Write);
  }


  void DxvkContext::emitGraphicsBarrier(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    // Emit barrier early so we can fold this into
    // the spill render pass barrier if possible
    if (srcStages | dstStages) {
      accessMemory(DxvkCmdBuffer::ExecBuffer,
        srcStages, srcAccess, dstStages, dstAccess);
    }

    this->spillRenderPass(true);

    // Flush barriers if there was no active render pass.
    // This is necessary because there are no resources
    // associated with the barrier to allow tracking.
    if (srcStages | dstStages)
      flushBarriers();
  }


  void DxvkContext::emitBufferBarrier(
    const Rc<DxvkBuffer>&           resource,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    this->spillRenderPass(true);

    accessBuffer(DxvkCmdBuffer::ExecBuffer,
      *resource, 0, resource->info().size,
      srcStages, srcAccess, dstStages, dstAccess,
      DxvkAccessOp::None);

    m_cmd->track(resource, DxvkAccess::Write);
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

    flushPendingAccesses(*resource, resource->getAvailableSubresources(), DxvkAccess::Write);

    accessImage(DxvkCmdBuffer::ExecBuffer,
      *resource, resource->getAvailableSubresources(),
      srcLayout, srcStages, srcAccess,
      dstLayout, dstStages, dstAccess,
      DxvkAccessOp::None);

    m_cmd->track(resource, DxvkAccess::Write);
  }


  void DxvkContext::generateMipmaps(
    const Rc<DxvkImageView>&        imageView,
          VkFilter                  filter) {
    if (imageView->info().mipCount <= 1)
      return;
    
    this->spillRenderPass(true);
    this->invalidateState();

    this->prepareImage(imageView->image(), imageView->imageSubresources());

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

    flushPendingAccesses(*imageView->image(), imageView->imageSubresources(), DxvkAccess::Write);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = imageView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, vk::makeLabel(0xe6dcf0,
        str::format("Mip gen (", dstName ? dstName : "unknown", ")").c_str()));
    }

    // Create image views, etc.
    DxvkMetaMipGenViews mipGenerator(imageView);
    
    VkImageLayout dstLayout = imageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkImageLayout srcLayout = imageView->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // If necessary, transition first mip level to the read-only layout
    addImageLayoutTransition(*imageView->image(),
      mipGenerator.getTopSubresource(), srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, false);

    addImageLayoutTransition(*imageView->image(),
      mipGenerator.getAllTargetSubresources(), dstLayout,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, true);

    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);
    
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
        addImageLayoutTransition(*imageView->image(),
          mipGenerator.getSourceSubresource(i), dstLayout,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, srcLayout,
          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          VK_ACCESS_2_SHADER_READ_BIT);

        flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);
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
      accessImage(DxvkCmdBuffer::ExecBuffer,
        *imageView->image(), imageView->imageSubresources(), srcLayout,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_SHADER_READ_BIT,
        DxvkAccessOp::None);
    } else {
      accessImage(DxvkCmdBuffer::ExecBuffer,
        *imageView->image(), mipGenerator.getAllSourceSubresources(), srcLayout,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_SHADER_READ_BIT,
        DxvkAccessOp::None);

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *imageView->image(), mipGenerator.getBottomSubresource(), dstLayout,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        DxvkAccessOp::None);
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(imageView->image(), DxvkAccess::Write);
    m_cmd->track(std::move(sampler));
  }
  
  
  void DxvkContext::invalidateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          Rc<DxvkResourceAllocation>&& slice) {
    Rc<DxvkResourceAllocation> prevAllocation = buffer->assignStorage(std::move(slice));
    m_cmd->track(std::move(prevAllocation));

    buffer->resetTracking();

    // We also need to update all bindings that the buffer
    // may be bound to either directly or through views.
    VkBufferUsageFlags usage = buffer->info().usage &
      ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

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
    if (buffer->canRelocate()) {
      buffer->enableStableAddress();

      m_common->memoryManager().lockResourceGpuAddress(buffer->storage());
    }
  }


  void DxvkContext::invalidateImage(
    const Rc<DxvkImage>&            image,
          Rc<DxvkResourceAllocation>&& slice) {
    invalidateImageWithUsage(image, std::move(slice), DxvkImageUsageInfo());
  }


  void DxvkContext::invalidateImageWithUsage(
    const Rc<DxvkImage>&            image,
          Rc<DxvkResourceAllocation>&& slice,
    const DxvkImageUsageInfo&       usageInfo) {
    VkImageUsageFlags usage = image->info().usage;

    // Invalidate active image descriptors
    if (usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT))
      m_descriptorState.dirtyViews(image->getShaderStages());

    // Ensure that the image is in its default layout before invalidation
    // and is not being tracked by the render pass layout logic. This does
    // assume that the new backing storage is also in the default layout.
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      bool found = false;

      for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments() && !found; i++)
        found = m_state.om.framebufferInfo.getAttachment(i).view->image() == image;

      if (found) {
        m_flags.set(DxvkContextFlag::GpDirtyFramebuffer);

        spillRenderPass(true);

        prepareImage(image, image->getAvailableSubresources());
      }
    }

    // If the image has any pending layout transitions, flush them accordingly.
    // There might be false positives here, but those do not affect correctness.
    if (resourceHasAccess(*image, image->getAvailableSubresources(), DxvkAccess::Write, DxvkAccessOp::None)) {
      spillRenderPass(true);

      flushBarriers();
    }

    // Actually replace backing storage and make sure to keep the old one alive
    Rc<DxvkResourceAllocation> prevAllocation = image->assignStorageWithUsage(std::move(slice), usageInfo);
    m_cmd->track(std::move(prevAllocation));

    if (usageInfo.stableGpuAddress)
      m_common->memoryManager().lockResourceGpuAddress(image->storage());

    image->resetTracking();
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
                                    && (!usageInfo.layout || image->info().layout == usageInfo.layout);

    // If everything matches already, no need to do anything. Only ensure
    // that the stable adress bit is respected if set for the first time.
    if (isUsageAndFormatCompatible && isAccessAndLayoutCompatible) {
      bool needsUpdate = (usageInfo.stableGpuAddress && image->canRelocate());

      if (usageInfo.colorSpace != VK_COLOR_SPACE_MAX_ENUM_KHR)
        needsUpdate |= (usageInfo.colorSpace != image->info().colorSpace);

      if (needsUpdate) {
        image->assignStorageWithUsage(image->storage(), usageInfo);
        m_common->memoryManager().lockResourceGpuAddress(image->storage());
      }

      return true;
    }

    // Ensure the image is accessible and in its default layout
    this->spillRenderPass(true);
    this->prepareImage(image, image->getAvailableSubresources());

    flushPendingAccesses(*image, image->getAvailableSubresources(), DxvkAccess::Write);

    if (isUsageAndFormatCompatible) {
      // Emit a barrier. If used in internal passes, this function
      // must be called *before* emitting dirty checks there.
      VkImageLayout oldLayout = image->info().layout;
      VkImageLayout newLayout = usageInfo.layout ? usageInfo.layout : oldLayout;

      image->assignStorageWithUsage(image->storage(), usageInfo);

      if (usageInfo.stableGpuAddress)
        m_common->memoryManager().lockResourceGpuAddress(image->storage());

      accessImage(DxvkCmdBuffer::ExecBuffer, *image, image->getAvailableSubresources(),
        oldLayout, image->info().stages, image->info().access,
        newLayout, image->info().stages, image->info().access,
        DxvkAccessOp::None);

      m_cmd->track(image, DxvkAccess::Write);
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

    auto storage = image->allocateStorageWithUsage(usage, 0u);

    DxvkRelocateImageInfo relocateInfo;
    relocateInfo.image = image;
    relocateInfo.storage = storage;
    relocateInfo.usageInfo = usage;

    relocateResources(0, nullptr, 1, &relocateInfo);
    return true;
  }


  template<bool Indexed, typename T>
  void DxvkContext::drawGeneric(
          uint32_t                  count,
    const T*                        draws) {
    if (this->commitGraphicsState<Indexed, false>()) {
      if (count == 1u) {
        // Most common case, just emit a single draw
        if constexpr (Indexed) {
          m_cmd->cmdDrawIndexed(draws->indexCount, draws->instanceCount,
            draws->firstIndex, draws->vertexOffset, draws->firstInstance);
        } else {
          m_cmd->cmdDraw(draws->vertexCount, draws->instanceCount,
            draws->firstVertex, draws->firstInstance);
        }

        m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);
      } else if (unlikely(needsDrawBarriers())) {
        // If the current pipeline has storage resource hazards,
        // unroll draws and insert a barrier after each one.
        for (uint32_t i = 0; i < count; i++) {
          if (i)
            this->commitGraphicsState<Indexed, false>();

          if constexpr (Indexed) {
            m_cmd->cmdDrawIndexed(draws[i].indexCount, draws[i].instanceCount,
              draws[i].firstIndex, draws[i].vertexOffset, draws[i].firstInstance);
          } else {
            m_cmd->cmdDraw(draws[i].vertexCount, draws[i].instanceCount,
              draws[i].firstVertex, draws[i].firstInstance);
          }
        }

        m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, count);
      } else {
        using MultiDrawInfo = std::conditional_t<Indexed,
          VkMultiDrawIndexedInfoEXT, VkMultiDrawInfoEXT>;

        // Intentially don't initialize this; we'll probably not use
        // the full batch size anyway, so doing so would be wasteful.
        std::array<MultiDrawInfo, DirectMultiDrawBatchSize> batch;

        uint32_t instanceCount = 0u;
        uint32_t instanceIndex = 0u;

        uint32_t batchSize = 0u;

        for (uint32_t i = 0; i < count; i++) {
          if (!batchSize) {
            instanceCount = draws[i].instanceCount;
            instanceIndex = draws[i].firstInstance;
          }

          if constexpr (Indexed) {
            auto& drawInfo = batch[batchSize++];
            drawInfo.firstIndex = draws[i].firstIndex;
            drawInfo.indexCount = draws[i].indexCount;
            drawInfo.vertexOffset = draws[i].vertexOffset;
          } else {
            auto& drawInfo = batch[batchSize++];
            drawInfo.firstVertex = draws[i].firstVertex;
            drawInfo.vertexCount = draws[i].vertexCount;
          }

          bool emitDraw = i + 1u == count || batchSize == DirectMultiDrawBatchSize;

          if (!emitDraw) {
            const auto& next = draws[i + 1u];

            emitDraw = instanceCount != next.instanceCount
                    || instanceIndex != next.firstInstance;
          }

          if (emitDraw) {
            if (m_features.test(DxvkContextFeature::DirectMultiDraw)) {
              if constexpr (Indexed) {
                m_cmd->cmdDrawMultiIndexed(batchSize, batch.data(),
                  instanceCount, instanceIndex);
              } else {
                m_cmd->cmdDrawMulti(batchSize, batch.data(),
                  instanceCount, instanceIndex);
              }
            } else {
              // This path only really exists for consistency reasons; all drivers
              // we care about support MultiDraw natively, but debug tools may not.
              if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
                const char* procName = Indexed ? "vkCmdDrawMultiIndexedEXT" : "vkCmdDrawMultiEXT";
                m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
                  vk::makeLabel(0u, str::format(procName, "(", batchSize, ")").c_str()));
              }

              for (uint32_t i = 0; i < batchSize; i++) {
                const auto& entry = batch[i];

                if constexpr (Indexed) {
                  m_cmd->cmdDrawIndexed(entry.indexCount, instanceCount,
                    entry.firstIndex, entry.vertexOffset, instanceIndex);
                } else {
                  m_cmd->cmdDraw(entry.vertexCount, instanceCount,
                    entry.firstVertex, instanceIndex);
                }
              }

              if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
                m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
            }

            m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);
            m_cmd->addStatCtr(DxvkStatCounter::CmdDrawsMerged, batchSize - 1u);

            batchSize = 0u;
          }
        }
      }
    }
  }


  template<bool Indexed>
  void DxvkContext::drawIndirectGeneric(
          VkDeviceSize              offset,
          uint32_t                  count,
          uint32_t                  stride,
          bool                      unroll) {
    constexpr VkDeviceSize elementSize = Indexed
      ? sizeof(VkDrawIndexedIndirectCommand)
      : sizeof(VkDrawIndirectCommand);

    if (this->commitGraphicsState<Indexed, true>()) {
      auto descriptor = m_state.id.argBuffer.getDescriptor();

      if (likely(count == 1u || !unroll || !needsDrawBarriers())) {
        if (Indexed) {
          m_cmd->cmdDrawIndexedIndirect(descriptor.buffer.buffer,
            descriptor.buffer.offset + offset, count, stride);
        } else {
          m_cmd->cmdDrawIndirect(descriptor.buffer.buffer,
            descriptor.buffer.offset + offset, count, stride);
        }

        m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

        if (unroll) {
          // Assume this is an automatically merged draw, if the app
          // explicitly uses multidraw then don't count it as merged.
          m_cmd->addStatCtr(DxvkStatCounter::CmdDrawsMerged, count - 1u);
        }

        if (unlikely(m_state.id.argBuffer.buffer()->hasGfxStores()))
          accessDrawBuffer(offset, count, stride, elementSize);
      } else {
        // If the pipeline has order-sensitive stores, submit one
        // draw at a time and insert barriers in between.
        for (uint32_t i = 0; i < count; i++) {
          if (i)
            this->commitGraphicsState<Indexed, true>();

          if (Indexed) {
            m_cmd->cmdDrawIndexedIndirect(descriptor.buffer.buffer,
              descriptor.buffer.offset + offset, 1u, 0u);
          } else {
            m_cmd->cmdDrawIndirect(descriptor.buffer.buffer,
              descriptor.buffer.offset + offset, 1u, 0u);
          }

          if (unlikely(m_state.id.argBuffer.buffer()->hasGfxStores()))
            accessDrawBuffer(offset, 1u, stride, elementSize);

          offset += stride;
        }

        m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, count);
      }
    }
  }


  template<bool Indexed>
  void DxvkContext::drawIndirectCountGeneric(
          VkDeviceSize          offset,
          VkDeviceSize          countOffset,
          uint32_t              maxCount,
          uint32_t              stride) {
    if (this->commitGraphicsState<Indexed, true>()) {
      auto argDescriptor = m_state.id.argBuffer.getDescriptor();
      auto cntDescriptor = m_state.id.cntBuffer.getDescriptor();

      if (Indexed) {
        m_cmd->cmdDrawIndexedIndirectCount(
          argDescriptor.buffer.buffer,
          argDescriptor.buffer.offset + offset,
          cntDescriptor.buffer.buffer,
          cntDescriptor.buffer.offset + countOffset,
          maxCount, stride);
      } else {
        m_cmd->cmdDrawIndirectCount(
          argDescriptor.buffer.buffer,
          argDescriptor.buffer.offset + offset,
          cntDescriptor.buffer.buffer,
          cntDescriptor.buffer.offset + countOffset,
          maxCount, stride);
      }

      m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

      if (unlikely(m_state.id.argBuffer.buffer()->hasGfxStores())) {
        accessDrawBuffer(offset, maxCount, stride, Indexed
          ? sizeof(VkDrawIndexedIndirectCommand)
          : sizeof(VkDrawIndirectCommand));
      }

      if (unlikely(m_state.id.cntBuffer.buffer()->hasGfxStores()))
        accessDrawCountBuffer(countOffset);
    }
  }


  void DxvkContext::resolveImage(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format,
          VkResolveModeFlagBits     mode,
          VkResolveModeFlagBits     stencilMode) {
    auto formatInfo = lookupFormatInfo(format);

    // Normalize resolve modes to available subresources
    VkImageAspectFlags aspects = region.srcSubresource.aspectMask
                               & region.dstSubresource.aspectMask;

    if (!(aspects & VK_IMAGE_ASPECT_STENCIL_BIT))
      stencilMode = VK_RESOLVE_MODE_NONE;

    // No-op, but legal
    if (!mode && !stencilMode)
      return;

    // Check whether the given resolve modes are supported for render pass resolves
    bool useFb = false;

    if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      const auto& properties = m_device->properties().vk12;

      useFb |= (properties.supportedDepthResolveModes   & mode)        != mode
            || (properties.supportedStencilResolveModes & stencilMode) != stencilMode;

      if (mode != stencilMode && (formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)) {
        useFb |= (!mode || !stencilMode)
          ? !properties.independentResolveNone
          : !properties.independentResolve;
      }
    } else {
      // For color images, only the default mode is supported
      useFb |= mode != getDefaultResolveMode(formatInfo);
    }

    // Also fall back to framebuffer path if this is a partial resolve,
    // or if two depth-stencil images are not format-compatible.
    if (!useFb) {
      useFb |= !dstImage->isFullSubresource(region.dstSubresource, region.extent)
            || !srcImage->isFullSubresource(region.srcSubresource, region.extent);

      if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
        useFb |= dstImage->info().format != srcImage->info().format;
    }

    // If the resolve format does not match the base image format, the resolve
    // attachment path is broken on some drivers so use the framebuffer path.
    if (m_device->perfHints().renderPassResolveFormatBug) {
      useFb |= format != srcImage->info().format
            || format != dstImage->info().format;
    }

    // Ensure that we can actually use the destination image as an attachment
    DxvkImageUsageInfo dstUsage = { };
    dstUsage.viewFormatCount = 1;
    dstUsage.viewFormats = &format;

    if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      dstUsage.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      dstUsage.stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dstUsage.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    } else {
      dstUsage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      dstUsage.stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstUsage.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    // Same for the source image, but check for shader usage
    // instead in case we need to use that path
    DxvkImageUsageInfo srcUsage = dstUsage;

    if (useFb) {
      srcUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      srcUsage.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      srcUsage.access = VK_ACCESS_SHADER_READ_BIT;
    }

    bool useHw = !ensureImageCompatibility(dstImage, dstUsage)
              || !ensureImageCompatibility(srcImage, srcUsage);

    // If possible, fold resolve into active render pass
    if (!useHw && !useFb && resolveImageInline(dstImage, srcImage, region, format, mode, stencilMode))
      return;

    this->spillRenderPass(true);
    this->prepareImage(dstImage, vk::makeSubresourceRange(region.dstSubresource));
    this->prepareImage(srcImage, vk::makeSubresourceRange(region.srcSubresource));

    if (unlikely(useHw)) {
      // Only used as a fallback if we can't use the images any other way,
      // format and resolve mode might not match what the app requests.
      this->resolveImageHw(dstImage, srcImage, region);
    } else if (unlikely(useFb)) {
      // Only used for weird resolve modes or partial resolves
      this->resolveImageFb(dstImage, srcImage,
        region, format, mode, stencilMode);
    } else {
      // Default path, use a resolve attachment
      this->resolveImageRp(dstImage, srcImage,
        region, format, mode, stencilMode);
    }
  }


  void DxvkContext::transformImage(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceRange&  dstSubresources,
          VkImageLayout             srcLayout,
          VkImageLayout             dstLayout) {
    this->spillRenderPass(false);

    if (srcLayout != dstLayout) {
      prepareImage(dstImage, dstSubresources);
      flushPendingAccesses(*dstImage, dstSubresources, DxvkAccess::Write);

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *dstImage, dstSubresources,
        srcLayout, dstImage->info().stages, dstImage->info().access,
        dstLayout, dstImage->info().stages, dstImage->info().access,
        DxvkAccessOp::None);
      
      m_cmd->track(dstImage, DxvkAccess::Write);
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

    // Completely ignore pure discards here if we can't fold them into the next
    // render pass, since all we'd do is add an extra barrier for no reason.
    if (attachmentIndex < 0 && !clearAspects)
      return;

    bool is3D = imageView->image()->info().type == VK_IMAGE_TYPE_3D;

    if ((clearAspects | discardAspects) == imageView->info().aspects && !is3D) {
      colorOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      depthOp.loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    if (attachmentIndex < 0) {
      bool useLateClear = m_device->perfHints().renderPassClearFormatBug
        && imageView->info().format != imageView->image()->info().format;

      flushPendingAccesses(*imageView, DxvkAccess::Write);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
        const char* imageName = imageView->image()->info().debugName;
        m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
          vk::makeLabel(0xe6f0dc, str::format("Clear render target (", imageName ? imageName : "unknown", ")").c_str()));
      }

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

        if (useLateClear && attachmentInfo.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
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

      addImageLayoutTransition(*imageView->image(), imageView->imageSubresources(),
        loadLayout, clearStages, 0, imageLayout, clearStages, clearAccess);
      flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

      m_cmd->cmdBeginRendering(&renderingInfo);

      if (useLateClear) {
        VkClearAttachment clearInfo = { };
        clearInfo.aspectMask = clearAspects;
        clearInfo.clearValue = clearValue;

        VkClearRect clearRect = { };
        clearRect.rect.extent.width = extent.width;
        clearRect.rect.extent.height = extent.height;
        clearRect.layerCount = imageView->info().layerCount;

        m_cmd->cmdClearAttachments(1, &clearInfo, 1, &clearRect);
      }

      m_cmd->cmdEndRendering();

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *imageView->image(), imageView->imageSubresources(),
        imageLayout, clearStages, clearAccess, storeLayout,
        imageView->image()->info().stages,
        imageView->image()->info().access, DxvkAccessOp::None);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

      m_cmd->track(imageView->image(), DxvkAccess::Write);
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


  void DxvkContext::preparePostRenderPassClears() {
    for (const auto& clear : m_deferredClears)
      prepareImage(clear.imageView->image(), clear.imageView->imageSubresources(), false);
  }


  void DxvkContext::hoistInlineClear(
          DxvkDeferredClear&        clear,
          VkRenderingAttachmentInfo& attachment,
          VkImageAspectFlagBits     aspect) {
    if (clear.clearAspects & aspect) {
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachment.clearValue = clear.clearValue;

      clear.clearAspects &= ~aspect;
    } else if (clear.discardAspects & aspect) {
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

      clear.discardAspects &= ~aspect;
    }
  }


  void DxvkContext::flushClearsInline() {
    small_vector<VkClearAttachment, MaxNumRenderTargets + 1u> attachments;

    for (auto& clear : m_deferredClears) {
      // If we end up here, the image is guaranteed to be bound and writable.
      int32_t attachmentIndex = m_state.om.framebufferInfo.findAttachment(clear.imageView);

      if (m_flags.test(DxvkContextFlag::GpRenderPassSecondaryCmd)) {
        // If the attachment hasn't been used for rendering yet, and if we're inside
        // a render pass using secondary command buffers, we can fold the clear or
        // discard into the render pass itself.
        if ((clear.clearAspects | clear.discardAspects) & VK_IMAGE_ASPECT_COLOR_BIT) {
          uint32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

          if (m_state.om.attachmentMask.getColorAccess(colorIndex) == DxvkAccess::None)
            hoistInlineClear(clear, m_state.om.renderingInfo.color[colorIndex], VK_IMAGE_ASPECT_COLOR_BIT);
        } else {
          if (m_state.om.attachmentMask.getDepthAccess() == DxvkAccess::None)
            hoistInlineClear(clear, m_state.om.renderingInfo.depth, VK_IMAGE_ASPECT_DEPTH_BIT);

          if (m_state.om.attachmentMask.getStencilAccess() == DxvkAccess::None)
            hoistInlineClear(clear, m_state.om.renderingInfo.stencil, VK_IMAGE_ASPECT_STENCIL_BIT);
        }
      }

      // Ignore discards here, we can't do anything useful with
      // those without interrupting the render pass again.
      if (!clear.clearAspects)
        continue;

      auto& entry = attachments.emplace_back();
      entry.aspectMask = clear.clearAspects;
      entry.clearValue = clear.clearValue;

      if (clear.clearAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
        entry.colorAttachment = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);
        m_state.om.attachmentMask.trackColorWrite(entry.colorAttachment);
      }

      if (clear.clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        m_state.om.attachmentMask.trackDepthWrite();

      if (clear.clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        m_state.om.attachmentMask.trackStencilWrite();
    }

    if (!attachments.empty()) {
      VkClearRect clearRect = { };
      clearRect.rect = m_state.om.renderingInfo.rendering.renderArea;
      clearRect.layerCount = m_state.om.renderingInfo.rendering.layerCount;

      m_cmd->cmdClearAttachments(attachments.size(), attachments.data(), 1u, &clearRect);
    }

    m_deferredClears.clear();
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


  void DxvkContext::flushRenderPassDiscards() {
    if (!m_deferredClears.empty()) {
      for (size_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
        auto view = m_state.om.framebufferInfo.getAttachment(i).view;

        if (m_deferredResolves.at(i).imageView)
          continue;

        for (const auto& clear : m_deferredClears) {
          if (clear.imageView->image() != view->image())
            continue;

          // Make sure that the cleared and discarded subresources are a superset
          // of the subresources currently active in the render pass
          auto clearSubresource = clear.imageView->subresources();
          auto renderSubresource = view->subresources();

          if (clearSubresource.aspectMask != renderSubresource.aspectMask
          || !vk::checkSubresourceRangeSuperset(clearSubresource, renderSubresource))
            continue;

          VkImageAspectFlags aspects = clear.clearAspects | clear.discardAspects;
          int32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(i);

          if (colorIndex < 0) {
            if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) && m_state.om.renderingInfo.rendering.pDepthAttachment)
              m_state.om.renderingInfo.depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) && m_state.om.renderingInfo.rendering.pStencilAttachment)
              m_state.om.renderingInfo.stencil.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
          } else if (aspects & VK_IMAGE_ASPECT_COLOR_BIT) {
            if (uint32_t(colorIndex) < m_state.om.renderingInfo.rendering.colorAttachmentCount)
              m_state.om.renderingInfo.color[colorIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
          }
        }
      }
    }
  }


  void DxvkContext::flushRenderPassResolves() {
    for (size_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
      auto& resolve = m_deferredResolves.at(i);

      if (!resolve.imageView)
        continue;

      // We can only fold the resolve into the render pass if all layers are
      // to be resolved.
      uint32_t layerMask = (2u << (m_state.om.renderingInfo.rendering.layerCount - 1u)) - 1u;

      if (resolve.layerMask != layerMask)
        continue;

      // Work out the image layout to use for the attachment based on image usage
      auto srcImage = m_state.om.framebufferInfo.getAttachment(i).view->image();
      auto dstImage = resolve.imageView->image();

      auto dstSubresource = resolve.imageView->imageSubresources();
      bool isDepthStencil = dstSubresource.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

      VkImageLayout oldLayout = dstImage->info().layout;
      VkImageLayout newLayout = dstImage->pickLayout(isDepthStencil
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      bool isFullWrite = (resolve.depthMode || !(dstSubresource.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT))
                      && (resolve.stencilMode || !(dstSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT));

      if (isFullWrite)
        oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      // If the application may have used the destination image as shader input in
      // any way, we need to preserve its contents throughout the render pass and
      // allocate new backing storage for the resolve attachment itself. This is
      // only safe to do if we are actually writing all destination subresources.
      VkPipelineStageFlags graphicsStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                                          | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                                          | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                                          | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                                          | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

      bool needsNewBackingStorage = (dstImage->info().stages & graphicsStages)
        && dstImage->isTracked(m_trackingId, DxvkAccess::Write);

      if (needsNewBackingStorage && dstImage->hasGfxStores()) {
        needsNewBackingStorage = resourceHasAccess(*dstImage, dstSubresource, DxvkAccess::Read, DxvkAccessOp::None)
                              || resourceHasAccess(*dstImage, dstSubresource, DxvkAccess::Write, DxvkAccessOp::None);
      }

      // Enable tracking so that we don't unnecessarily hit slow paths in the future
      if (dstImage->info().stages & graphicsStages)
        dstImage->trackGfxStores();

      if (needsNewBackingStorage) {
        auto imageSubresource = dstImage->getAvailableSubresources();

        if (dstSubresource != imageSubresource || !isFullWrite || !dstImage->canRelocate())
          continue;

        // Allocate and assign new backing storage. Deliberately don't go through
        // invalidateImageWithUsage here since we know we only need a subset of
        // state invalidations here and that method may mess with render passes.
        VkFormat format = resolve.imageView->info().format;

        DxvkImageUsageInfo usageInfo = { };
        usageInfo.usage = isDepthStencil
          ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
          : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usageInfo.viewFormatCount = 1;
        usageInfo.viewFormats = &format;

        auto newStorage = dstImage->allocateStorageWithUsage(usageInfo, 0u);
        auto oldStorage = dstImage->assignStorageWithUsage(std::move(newStorage), usageInfo);

        m_descriptorState.dirtyViews(dstImage->getShaderStages());

        dstImage->resetTracking();

        m_cmd->track(std::move(oldStorage));
      }

      // Record layout transition from default layout to attachment layout
      VkPipelineStageFlags2 stages = isDepthStencil
        ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
        : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

      VkAccessFlags2 access = isDepthStencil
        ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

      addImageLayoutTransition(*dstImage, dstSubresource,
        oldLayout, dstImage->info().stages, dstImage->info().access,
        newLayout, stages, access);

      // Record layout transition from attachment layout back to default
      // layout. This will be flushed after the render pass has ended.
      accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage,
        dstSubresource, newLayout, stages, access, DxvkAccessOp::None);

      if (!isDepthStencil) {
        uint32_t index = m_state.om.framebufferInfo.getColorAttachmentIndex(i);

        auto& color = m_state.om.renderingInfo.color[index];
        color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color.resolveImageView = resolve.imageView->handle();
        color.resolveImageLayout = newLayout;
      } else {
        if (resolve.depthMode) {
          auto& depth = m_state.om.renderingInfo.depth;
          depth.resolveMode = resolve.depthMode;
          depth.resolveImageView = resolve.imageView->handle();
          depth.resolveImageLayout = newLayout;
        }

        if (resolve.stencilMode) {
          auto& stencil = m_state.om.renderingInfo.stencil;
          stencil.resolveMode = resolve.stencilMode;
          stencil.resolveImageView = resolve.imageView->handle();
          stencil.resolveImageLayout = newLayout;
        }
      }

      m_cmd->track(dstImage, DxvkAccess::Write);
      m_cmd->track(srcImage, DxvkAccess::Read);

      // Reset deferred resolve state so we don't do a
      // redundant resolve after the render pass here
      resolve = DxvkDeferredResolve();
    }

    // Transition all resolve attachments to the desired layout
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);
  }


  void DxvkContext::flushResolves() {
    for (size_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
      auto& resolve = m_deferredResolves.at(i);

      if (!resolve.imageView)
        continue;

      // Queue up normal resolves here, the render pass has already ended.
      const auto& attachment = m_state.om.framebufferInfo.getAttachment(i);

      auto srcSubresource = attachment.view->imageSubresources();
      auto dstSubresource = resolve.imageView->imageSubresources();

      // We're within a render pass, any pending clears will have happened
      // after the resolve, so ignore them here.
      prepareImage(attachment.view->image(), srcSubresource, false);
      prepareImage(resolve.imageView->image(), dstSubresource, false);

      while (resolve.layerMask) {
        uint32_t layerIndex = bit::tzcnt(resolve.layerMask);
        uint32_t layerCount = bit::tzcnt(~(resolve.layerMask >> layerIndex));

        VkImageResolve region = { };
        region.dstSubresource.aspectMask = dstSubresource.aspectMask;
        region.dstSubresource.mipLevel = dstSubresource.baseMipLevel;
        region.dstSubresource.baseArrayLayer = dstSubresource.baseArrayLayer + layerIndex;
        region.dstSubresource.layerCount = layerCount;
        region.srcSubresource.aspectMask = srcSubresource.aspectMask;
        region.srcSubresource.mipLevel = srcSubresource.baseMipLevel;
        region.srcSubresource.baseArrayLayer = srcSubresource.baseArrayLayer + layerIndex;
        region.srcSubresource.layerCount = layerCount;
        region.extent = resolve.imageView->mipLevelExtent(0u);

        resolveImageRp(resolve.imageView->image(), attachment.view->image(),
          region, attachment.view->info().format, resolve.depthMode, resolve.stencilMode);

        resolve.layerMask &= ~0u << (layerIndex + layerCount);
      }

      // Reset deferred resolve state
      resolve = DxvkDeferredResolve();
    }
  }


  void DxvkContext::finalizeLoadStoreOps() {
    auto& renderingInfo = m_state.om.renderingInfo;

    // Track attachment access for render pass clears and resolves
    for (uint32_t i = 0; i < renderingInfo.rendering.colorAttachmentCount; i++) {
      if (renderingInfo.color[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        m_state.om.attachmentMask.trackColorWrite(i);
      if (renderingInfo.color[i].resolveImageView && renderingInfo.color[i].resolveMode)
        m_state.om.attachmentMask.trackColorRead(i);
    }

    if (renderingInfo.rendering.pDepthAttachment) {
      if (renderingInfo.depth.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        m_state.om.attachmentMask.trackDepthWrite();
      if (renderingInfo.depth.resolveImageView && renderingInfo.depth.resolveMode)
        m_state.om.attachmentMask.trackDepthRead();
    }

    if (renderingInfo.rendering.pStencilAttachment) {
      if (renderingInfo.stencil.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        m_state.om.attachmentMask.trackStencilWrite();
      if (renderingInfo.stencil.resolveImageView && renderingInfo.stencil.resolveMode)
        m_state.om.attachmentMask.trackStencilRead();
    }

    // If we don't have maintenance7 support, we need to pretend that accessing
    // one of depth or stencil also accesses the other aspect in the same way
    if (!m_device->properties().khrMaintenance7.separateDepthStencilAttachmentAccess)
      m_state.om.attachmentMask.unifyDepthStencilAccess();

    // Use attachment access info to set the final load/store ops
    for (uint32_t i = 0; i < renderingInfo.rendering.colorAttachmentCount; i++) {
      adjustAttachmentLoadStoreOps(renderingInfo.color[i],
        m_state.om.attachmentMask.getColorAccess(i));
    }

    if (renderingInfo.rendering.pDepthAttachment) {
      adjustAttachmentLoadStoreOps(renderingInfo.depth,
        m_state.om.attachmentMask.getDepthAccess());
    }

    if (renderingInfo.rendering.pStencilAttachment) {
      adjustAttachmentLoadStoreOps(renderingInfo.stencil,
        m_state.om.attachmentMask.getStencilAccess());
    }
  }


  void DxvkContext::adjustAttachmentLoadStoreOps(
          VkRenderingAttachmentInfo&  attachment,
          DxvkAccess                  access) const {
    if (access == DxvkAccess::None) {
      // If the attachment is not accessed at all, we can set both the
      // load and store op to NONE if supported by the implementation.
      bool hasLoadOpNone = m_device->features().khrLoadStoreOpNone;

      attachment.loadOp = hasLoadOpNone
        ? VK_ATTACHMENT_LOAD_OP_NONE
        : VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
    } else if (access == DxvkAccess::Read) {
      // Unlike clears, we don't treat DONT_CARE as a write. If the
      // attachment isn't written in this pass but is read anyway,
      // demote the store op to DONT_CARE as well.
      if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      else
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
    }
  }


  void DxvkContext::updateBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
    const void*                     data) {
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    if (!prepareOutOfOrderTransfer(buffer, offset, size, DxvkAccess::Write)) {
      spillRenderPass(true);

      flushPendingAccesses(*buffer, offset, size, DxvkAccess::Write);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    auto bufferSlice = buffer->getSliceHandle(offset, size);

    m_cmd->cmdUpdateBuffer(cmdBuffer,
      bufferSlice.handle,
      bufferSlice.offset,
      bufferSlice.length,
      data);

    accessBuffer(cmdBuffer, *buffer, offset, size,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);

    m_cmd->track(buffer, DxvkAccess::Write);
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

    accessBufferTransfer(*buffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    m_cmd->track(source, DxvkAccess::Read);
    m_cmd->track(buffer, DxvkAccess::Write);
  }


  void DxvkContext::uploadImage(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset,
          VkDeviceSize              subresourceAlignment,
          VkFormat                  format) {
    // Always use framebuffer path for depth-stencil images since we know
    // they are writeable and can't use Vulkan transfer queues. Stencil
    // data is interleaved and needs to be decoded manually anyway.
    bool useFb = !formatsAreCopyCompatible(image->info().format, format);

    if (useFb)
      uploadImageFb(image, source, sourceOffset, subresourceAlignment, format);
    else
      uploadImageHw(image, source, sourceOffset, subresourceAlignment);
  }


  void DxvkContext::setViewports(
          uint32_t            viewportCount,
    const DxvkViewport*       viewports) {
    for (uint32_t i = 0; i < viewportCount; i++) {
      m_state.vp.viewports[i] = viewports[i].viewport;
      m_state.vp.scissorRects[i] = viewports[i].scissor;

      // Vulkan viewports are not allowed to have a width or
      // height of zero, so we fall back to a dummy viewport
      // and instead set an empty scissor rect, which is legal.
      if (viewports[i].viewport.width <= 0.0f || viewports[i].viewport.height == 0.0f) {
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
      ia.primitiveTopology(),
      ia.primitiveRestart(),
      ia.patchVertexCount());
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setInputLayout(
          uint32_t             attributeCount,
    const DxvkVertexInput*     attributes,
          uint32_t             bindingCount,
    const DxvkVertexInput*     bindings) {
    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers);

    for (uint32_t i = 0; i < bindingCount; i++) {
      auto binding = bindings[i].binding();

      m_state.gp.state.ilBindings[i] = DxvkIlBinding(
        binding.binding, 0,
        binding.inputRate,
        binding.divisor);
      m_state.vi.vertexExtents[i] = binding.extent;
    }

    for (uint32_t i = bindingCount; i < m_state.gp.state.il.bindingCount(); i++) {
      m_state.gp.state.ilBindings[i] = DxvkIlBinding();
      m_state.vi.vertexExtents[i] = 0;
    }

    for (uint32_t i = 0; i < attributeCount; i++) {
      auto attribute = attributes[i].attribute();

      m_state.gp.state.ilAttributes[i] = DxvkIlAttribute(
        attribute.location,
        attribute.binding,
        attribute.format,
        attribute.offset);
    }

    for (uint32_t i = attributeCount; i < m_state.gp.state.il.attributeCount(); i++)
      m_state.gp.state.ilAttributes[i] = DxvkIlAttribute();

    m_state.gp.state.il = DxvkIlInfo(attributeCount, bindingCount);
  }


  void DxvkContext::setRasterizerState(const DxvkRasterizerState& rs) {
    VkCullModeFlags cullMode = rs.cullMode();
    VkFrontFace frontFace = rs.frontFace();

    if (m_state.dyn.cullMode != cullMode || m_state.dyn.frontFace != frontFace) {
      m_state.dyn.cullMode = cullMode;
      m_state.dyn.frontFace = frontFace;

      m_flags.set(DxvkContextFlag::GpDirtyRasterizerState);
    }

    if (unlikely(rs.sampleCount() != m_state.gp.state.rs.sampleCount())) {
      if (!m_state.gp.state.ms.sampleCount())
        m_flags.set(DxvkContextFlag::GpDirtyMultisampleState);

      if (!m_features.test(DxvkContextFeature::VariableMultisampleRate))
        m_flags.set(DxvkContextFlag::GpRenderPassNeedsFlush);
    }

    DxvkRsInfo rsInfo(
      rs.depthClip(),
      rs.depthBias(),
      rs.polygonMode(),
      rs.sampleCount(),
      rs.conservativeMode(),
      rs.flatShading(),
      rs.lineMode());

    if (!m_state.gp.state.rs.eq(rsInfo)) {
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);

      // Since depth bias enable is only dynamic for base pipelines,
      // it is applied as part of the dynamic depth-stencil state
      if (m_state.gp.state.rs.depthBiasEnable() != rs.depthBias())
        m_flags.set(DxvkContextFlag::GpDirtyDepthStencilState);

      m_state.gp.state.rs = rsInfo;
    }
  }
  
  
  void DxvkContext::setMultisampleState(const DxvkMultisampleState& ms) {
    m_state.gp.state.ms = DxvkMsInfo(
      m_state.gp.state.ms.sampleCount(),
      ms.sampleMask(),
      ms.alphaToCoverage());

    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyMultisampleState);
  }
  
  
  void DxvkContext::setDepthStencilState(const DxvkDepthStencilState& ds) {
    m_state.gp.state.ds = DxvkDsInfo(
      ds.depthTest(),
      ds.depthWrite(),
      m_state.gp.state.ds.enableDepthBoundsTest(),
      ds.stencilTest(),
      ds.depthCompareOp());

    DxvkStencilOp front = ds.stencilOpFront();

    m_state.gp.state.dsFront = DxvkDsStencilOp(
      front.failOp(),
      front.passOp(),
      front.depthFailOp(),
      front.compareOp(),
      front.compareMask(),
      front.writeMask());

    DxvkStencilOp back = ds.stencilOpBack();

    m_state.gp.state.dsBack = DxvkDsStencilOp(
      back.failOp(),
      back.passOp(),
      back.depthFailOp(),
      back.compareOp(),
      back.compareMask(),
      back.writeMask());

    m_flags.set(
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyDepthStencilState);
  }
  
  
  void DxvkContext::setLogicOpState(const DxvkLogicOpState& lo) {
    m_state.gp.state.om = DxvkOmInfo(
      lo.logicOpEnable(),
      lo.logicOp(),
      m_state.gp.state.om.feedbackLoop());
    
    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }
  
  
  void DxvkContext::setBlendMode(
          uint32_t            attachment,
    const DxvkBlendMode&      blendMode) {
    m_state.gp.state.omBlend[attachment] = DxvkOmAttachmentBlend(
      blendMode.blendEnable(),
      blendMode.colorSrcFactor(),
      blendMode.colorDstFactor(),
      blendMode.colorBlendOp(),
      blendMode.alphaSrcFactor(),
      blendMode.alphaDstFactor(),
      blendMode.alphaBlendOp(),
      blendMode.writeMask());

    m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
  }


  void DxvkContext::setBarrierControl(DxvkBarrierControlFlags control) {
    // If any currently relevant control flags change, play it safe and force
    // a barrier the next time we encounter a write-after-write hazard, even
    // if the same set of flags is restored by that time. Only check graphics
    // flags inside a render pass to avoid performance regressions when an
    // application uses this feature but we already have an app profile.
    // Barriers get flushed when beginning or ending a render pass anyway.
    DxvkBarrierControlFlags mask = m_flags.test(DxvkContextFlag::GpRenderPassBound)
      ? DxvkBarrierControlFlags(DxvkBarrierControl::GraphicsAllowReadWriteOverlap)
      : DxvkBarrierControlFlags(DxvkBarrierControl::ComputeAllowReadWriteOverlap,
                                DxvkBarrierControl::ComputeAllowWriteOnlyOverlap);

    if (!((m_barrierControl ^ control) & mask).isClear()) {
      m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl);
    }

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

    m_cmd->track(bindInfo.dstResource, DxvkAccess::Write);
  }


  void DxvkContext::signalGpuEvent(const Rc<DxvkEvent>& event) {
    this->spillRenderPass(true);
    
    Rc<DxvkGpuEvent> gpuEvent = m_common->eventPool().allocEvent();
    event->assignGpuEvent(gpuEvent);

    // Supported client APIs can't access device memory in a defined manner
    // without triggering a queue submission first, so we really only need
    // to wait for prior commands, especially queries, to complete.
    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    m_cmd->cmdSetEvent(gpuEvent->handle(), &depInfo);
    m_cmd->track(std::move(gpuEvent));
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

    accessMemory(DxvkCmdBuffer::ExecBuffer, srcStages, srcAccess,
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    flushBarriers();

    m_cmd->cmdLaunchCuKernel(nvxLaunchInfo);

    for (auto& r : buffers) {
      VkAccessFlags accessFlags = (r.second.test(DxvkAccess::Read) * VK_ACCESS_SHADER_READ_BIT)
                                | (r.second.test(DxvkAccess::Write) * VK_ACCESS_SHADER_WRITE_BIT);

      accessBuffer(DxvkCmdBuffer::ExecBuffer,
        *r.first, 0, r.first->info().size,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        accessFlags, DxvkAccessOp::None);

      m_cmd->track(r.first, r.second.test(DxvkAccess::Write) ? DxvkAccess::Write : DxvkAccess::Read);
    }

    for (auto& r : images) {
      VkAccessFlags accessFlags = (r.second.test(DxvkAccess::Read) * VK_ACCESS_SHADER_READ_BIT)
                                | (r.second.test(DxvkAccess::Write) * VK_ACCESS_SHADER_WRITE_BIT);
      accessImage(DxvkCmdBuffer::ExecBuffer, *r.first,
        r.first->getAvailableSubresources(), r.first->info().layout,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, accessFlags, DxvkAccessOp::None);

      m_cmd->track(r.first, r.second.test(DxvkAccess::Write) ? DxvkAccess::Write : DxvkAccess::Read);
    }
  }
  
  
  void DxvkContext::writeTimestamp(const Rc<DxvkQuery>& query) {
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


  bool DxvkContext::needsDrawBarriers() {
    return m_state.gp.flags.test(DxvkGraphicsPipelineFlag::UnrollMergedDraws)
      && !m_barrierControl.test(DxvkBarrierControl::GraphicsAllowReadWriteOverlap);
  }


  void DxvkContext::beginRenderPassDebugRegion() {
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    bool hasColorAttachments = false;
    bool hasDepthAttachment = m_state.om.renderTargets.depth.view != nullptr;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      hasColorAttachments |= m_state.om.renderTargets.color[i].view != nullptr;

    std::stringstream label;

    if (hasColorAttachments)
      label << "Color";
    else if (hasDepthAttachment)
      label << "Depth";
    else
      label << "Render";

    label << " pass " << uint32_t(++m_renderPassIndex) << " (";

    hasColorAttachments = false;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_state.om.renderTargets.color[i].view) {
        const char* imageName = m_state.om.renderTargets.color[i].view->image()->info().debugName;
        label << (hasColorAttachments ? ", " : "") << i << ": " << (imageName ? imageName : "unknown");

        hasColorAttachments = true;
        sampleCount = m_state.om.renderTargets.color[i].view->image()->info().sampleCount;
      }
    }

    if (m_state.om.renderTargets.depth.view) {
      if (hasColorAttachments)
        label << ", ";

      const char* imageName = m_state.om.renderTargets.depth.view->image()->info().debugName;
      label << "DS:" << (imageName ? imageName : "unknown");

      sampleCount = m_state.om.renderTargets.depth.view->image()->info().sampleCount;
    }

    if (!hasColorAttachments && !hasDepthAttachment)
      label << "No attachments";

    if (sampleCount > VK_SAMPLE_COUNT_1_BIT)
      label << ", " << uint32_t(sampleCount) << "x MSAA";

    label << ")";

    uint32_t color = sampleCount > VK_SAMPLE_COUNT_1_BIT ? 0xf0dcf0 : 0xf0e6dc;

    pushDebugRegion(vk::makeLabel(color, label.str().c_str()),
      util::DxvkDebugLabelType::InternalRenderPass);
  }


  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::beginBarrierControlDebugRegion() {
    if (hasDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl))
      return;

    const char* label = nullptr;

    if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      if (m_barrierControl.test(DxvkBarrierControl::ComputeAllowReadWriteOverlap))
        label = "Relaxed sync";
      else if (m_barrierControl.test(DxvkBarrierControl::ComputeAllowWriteOnlyOverlap))
        label = "Relaxed sync (write-only)";
    } else {
      if (m_barrierControl.test(DxvkBarrierControl::GraphicsAllowReadWriteOverlap))
        label = "Relaxed sync";
    }

    if (label) {
      pushDebugRegion(vk::makeLabel(0x9bded9, label),
        util::DxvkDebugLabelType::InternalBarrierControl);
    }
  }


  void DxvkContext::beginDebugLabel(const VkDebugUtilsLabelEXT& label) {
    if (m_features.test(DxvkContextFeature::DebugUtils))
      pushDebugRegion(label, util::DxvkDebugLabelType::External);
  }


  void DxvkContext::endDebugLabel() {
    if (m_features.test(DxvkContextFeature::DebugUtils))
      popDebugRegion(util::DxvkDebugLabelType::External);
  }


  void DxvkContext::insertDebugLabel(const VkDebugUtilsLabelEXT& label) {
    if (m_features.test(DxvkContextFeature::DebugUtils))
      m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, label);
  }


  void DxvkContext::setDebugName(const Rc<DxvkPagedResource>& resource, const char* name) {
    if (m_features.test(DxvkContextFeature::DebugUtils))
      resource->setDebugName(name);
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

    flushPendingAccesses(*dstView, DxvkAccess::Write);
    flushPendingAccesses(*srcView, DxvkAccess::Read);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstView->image()->info().debugName;
      const char* srcName = srcView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Blit (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

    VkImageLayout srcLayout = srcView->image()->pickLayout(srcIsDepthStencil
      ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkImageLayout dstLayout = dstView->image()->pickLayout(
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    addImageLayoutTransition(*dstView->image(), dstView->imageSubresources(),
      dstLayout, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, false);
    addImageLayoutTransition(*srcView->image(), srcView->imageSubresources(),
      srcLayout, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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
    accessImage(DxvkCmdBuffer::ExecBuffer,
      *dstView->image(), dstView->imageSubresources(), dstLayout,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, DxvkAccessOp::None);
    
    accessImage(DxvkCmdBuffer::ExecBuffer,
      *srcView->image(), srcView->imageSubresources(), srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(dstView->image(), DxvkAccess::Write);
    m_cmd->track(srcView->image(), DxvkAccess::Read);
    m_cmd->track(std::move(sampler));
  }


  void DxvkContext::blitImageHw(
    const Rc<DxvkImageView>&    dstView,
    const VkOffset3D*           dstOffsets,
    const Rc<DxvkImageView>&    srcView,
    const VkOffset3D*           srcOffsets,
          VkFilter              filter) {
    flushPendingAccesses(*dstView, DxvkAccess::Write);
    flushPendingAccesses(*srcView, DxvkAccess::Read);

    // Prepare the two images for transfer ops if necessary
    auto dstLayout = dstView->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    auto srcLayout = srcView->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    addImageLayoutTransition(*dstView->image(), dstView->imageSubresources(),
      dstLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, false);
    addImageLayoutTransition(*srcView->image(), srcView->imageSubresources(),
      srcLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessImage(DxvkCmdBuffer::ExecBuffer, *dstView->image(), dstView->imageSubresources(), dstLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);

    accessImage(DxvkCmdBuffer::ExecBuffer, *srcView->image(), srcView->imageSubresources(), srcLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);

    m_cmd->track(dstView->image(), DxvkAccess::Write);
    m_cmd->track(srcView->image(), DxvkAccess::Read);
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

        // Fix up some edge cases with unaligned mips of block-compressed images
        VkExtent3D maxExtent = image->mipLevelExtent(imageSubresource.mipLevel, aspect);

        copyRegion.imageExtent = VkExtent3D {
          std::min(copyRegion.imageExtent.width,  maxExtent.width  - copyRegion.imageOffset.x),
          std::min(copyRegion.imageExtent.height, maxExtent.height - copyRegion.imageOffset.y),
          std::min(copyRegion.imageExtent.depth,  maxExtent.depth  - copyRegion.imageOffset.z) };

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
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    VkDeviceSize dataSize = imageSubresource.layerCount * util::computeImageDataSize(
      image->info().format, imageExtent, imageSubresource.aspectMask);

    // We may copy to only one aspect at a time, but pipeline
    // barriers need to have all available aspect bits set
    auto dstFormatInfo = image->formatInfo();

    auto dstSubresource = imageSubresource;
    dstSubresource.aspectMask = dstFormatInfo->aspectMask;

    if (!prepareOutOfOrderTransfer(buffer, bufferOffset, dataSize, DxvkAccess::Read)
     || !prepareOutOfOrderTransfer(image, DxvkAccess::Write)) {
      spillRenderPass(true);
      prepareImage(image, vk::makeSubresourceRange(imageSubresource));

      flushPendingAccesses(*image, dstSubresource, imageOffset, imageExtent, DxvkAccess::Write);
      flushPendingAccesses(*buffer, bufferOffset, dataSize, DxvkAccess::Read);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    auto bufferSlice = buffer->getSliceHandle(bufferOffset, dataSize);

    // Initialize the image if the entire subresource is covered
    VkImageLayout dstImageLayoutTransfer = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    addImageLayoutTransition(*image, vk::makeSubresourceRange(dstSubresource), dstImageLayoutTransfer,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      image->isFullSubresource(imageSubresource, imageExtent));
    flushImageLayoutTransitions(cmdBuffer);

    copyImageBufferData<true>(cmdBuffer,
      image, imageSubresource, imageOffset, imageExtent, dstImageLayoutTransfer,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);

    accessImageRegion(cmdBuffer, *image, dstSubresource, imageOffset, imageExtent, dstImageLayoutTransfer,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);

    accessBuffer(cmdBuffer, *buffer, bufferOffset, dataSize,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);

    m_cmd->track(image, DxvkAccess::Write);
    m_cmd->track(buffer, DxvkAccess::Read);
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

    flushPendingAccesses(*image, vk::makeSubresourceRange(imageSubresource), DxvkAccess::Write);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = image->info().debugName;
      const char* srcName = buffer->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Upload image (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

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

    flushPendingAccesses(*bufferView, DxvkAccess::Read);

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

    addImageLayoutTransition(*image, vk::makeSubresourceRange(imageSubresource),
      imageLayout, stages, access, discard);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessImage(DxvkCmdBuffer::ExecBuffer,
      *image, vk::makeSubresourceRange(imageSubresource),
      imageLayout, stages, access, DxvkAccessOp::None);

    accessBuffer(DxvkCmdBuffer::ExecBuffer, *bufferView,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(image, DxvkAccess::Write);
    m_cmd->track(buffer, DxvkAccess::Read);
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

    auto srcSubresource = imageSubresource;
    srcSubresource.aspectMask = srcFormatInfo->aspectMask;

    flushPendingAccesses(*image, srcSubresource, imageOffset, imageExtent, DxvkAccess::Read);
    flushPendingAccesses(*buffer, bufferOffset, dataSize, DxvkAccess::Write);

    // Select a suitable image layout for the transfer op
    VkImageLayout srcImageLayoutTransfer = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    addImageLayoutTransition(*image, vk::makeSubresourceRange(srcSubresource), srcImageLayoutTransfer,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

    this->copyImageBufferData<false>(DxvkCmdBuffer::ExecBuffer,
      image, imageSubresource, imageOffset, imageExtent, srcImageLayoutTransfer,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);

    accessImageRegion(DxvkCmdBuffer::ExecBuffer, *image, srcSubresource, imageOffset, imageExtent, srcImageLayoutTransfer,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);

    accessBuffer(DxvkCmdBuffer::ExecBuffer, *buffer, bufferOffset, dataSize,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);

    m_cmd->track(buffer, DxvkAccess::Write);
    m_cmd->track(image, DxvkAccess::Read);
  }


  void DxvkContext::copyImageToBufferCs(
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

    flushPendingAccesses(*image, imageSubresource,
      imageOffset, imageExtent, DxvkAccess::Read);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = buffer->info().debugName;
      const char* srcName = image->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Readback image (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

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

    flushPendingAccesses(*bufferView, DxvkAccess::Write);

    // Transition image to a layout we can use for reading as necessary
    VkImageLayout imageLayout = (image->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? image->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      : image->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    addImageLayoutTransition(*image, vk::makeSubresourceRange(imageSubresource),
      imageLayout, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessBuffer(DxvkCmdBuffer::ExecBuffer, *bufferView,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, DxvkAccessOp::None);

    accessImageRegion(DxvkCmdBuffer::ExecBuffer, *image, imageSubresource, imageOffset, imageExtent, imageLayout,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, DxvkAccessOp::None);

    m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(buffer, DxvkAccess::Write);
    m_cmd->track(image, DxvkAccess::Read);
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
      this->spillRenderPass(true);

      this->prepareImage(imageView->image(), imageView->imageSubresources());
      this->flushPendingAccesses(*imageView->image(), imageView->imageSubresources(), DxvkAccess::Write);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
        const char* dstName = imageView->image()->info().debugName;

        m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, vk::makeLabel(0xf0dcdc,
          str::format("Clear view (", dstName ? dstName : "unknown", ")").c_str()));
      }

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

      addImageLayoutTransition(*imageView->image(), imageView->imageSubresources(),
        clearLayout, clearStages, clearAccess, false);
      flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

      // We cannot leverage render pass clears
      // because we clear only part of the view
      m_cmd->cmdBeginRendering(&renderingInfo);
    } else {
      // Make sure the render pass is active so
      // that we can actually perform the clear
      this->startRenderPass();

      if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
        uint32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);
        m_state.om.attachmentMask.trackColorWrite(colorIndex);
      }

      if (aspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        m_state.om.attachmentMask.trackDepthWrite();

      if (aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        m_state.om.attachmentMask.trackStencilWrite();
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

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *imageView->image(), imageView->imageSubresources(),
        clearLayout, clearStages, clearAccess, DxvkAccessOp::None);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

      m_cmd->track(imageView->image(), DxvkAccess::Write);
    }
  }

  
  void DxvkContext::clearImageViewCs(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkClearValue          value) {
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    if (!prepareOutOfOrderTransfer(imageView->image(), DxvkAccess::Write)) {
      spillRenderPass(true);
      invalidateState();

      prepareImage(imageView->image(), imageView->imageSubresources());
      flushPendingAccesses(*imageView->image(), imageView->imageSubresources(), DxvkAccess::Write);

      cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = imageView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(cmdBuffer, vk::makeLabel(0xf0dcdc,
        str::format("Clear view (", dstName ? dstName : "unknown", ")").c_str()));
    }

    // Avoid inserting useless barriers if the image is already in the correct layout
    if (imageView->image()->info().layout != VK_IMAGE_LAYOUT_GENERAL
     || !imageView->image()->isInitialized(imageView->imageSubresources())) {
      addImageLayoutTransition(*imageView->image(), imageView->imageSubresources(),
        VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
        imageView->image()->isFullSubresource(vk::pickSubresourceLayers(imageView->imageSubresources(), 0), extent));
      flushImageLayoutTransitions(cmdBuffer);
    }

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
    
    m_cmd->cmdBindPipeline(cmdBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);
    m_cmd->cmdBindDescriptorSet(cmdBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeLayout,
      descriptorSet, 0, nullptr);
    m_cmd->cmdPushConstants(cmdBuffer,
      pipeInfo.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushArgs), &pushArgs);
    m_cmd->cmdDispatch(cmdBuffer,
      workgroups.width, workgroups.height, workgroups.depth);

    accessImage(cmdBuffer, *imageView->image(), imageView->imageSubresources(),
      VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_WRITE_BIT, DxvkAccessOp::None);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(cmdBuffer);

    m_cmd->track(imageView->image(), DxvkAccess::Write);
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

    // If we copy between disjoint regions of the same image subresources,
    // make sure that we only do one single transition to GENERAL.
    bool hasOvelap = dstImage == srcImage && vk::checkSubresourceRangeOverlap(dstSubresourceRange, srcSubresourceRange);

    VkImageSubresourceRange overlapSubresourceRange = { };
    overlapSubresourceRange.aspectMask = srcSubresource.aspectMask | dstSubresource.aspectMask;
    overlapSubresourceRange.baseArrayLayer = std::min(srcSubresource.baseArrayLayer, dstSubresource.baseArrayLayer);
    overlapSubresourceRange.baseMipLevel = srcSubresource.mipLevel;
    overlapSubresourceRange.layerCount = std::max(srcSubresource.baseArrayLayer, dstSubresource.baseArrayLayer)
      + dstSubresource.layerCount - overlapSubresourceRange.baseArrayLayer;
    overlapSubresourceRange.levelCount = 1u;

    flushPendingAccesses(*dstImage, dstSubresource, dstOffset, extent, DxvkAccess::Write);
    flushPendingAccesses(*srcImage, srcSubresource, srcOffset, extent, DxvkAccess::Read);

    VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    if (hasOvelap) {
      addImageLayoutTransition(*dstImage, overlapSubresourceRange, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, false);
    } else {
      dstImageLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      srcImageLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

      addImageLayoutTransition(*dstImage, dstSubresourceRange, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        dstImage->isFullSubresource(dstSubresource, extent));
      addImageLayoutTransition(*srcImage, srcSubresourceRange, srcImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    }

    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    if (hasOvelap) {
      accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage, overlapSubresourceRange, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);
    } else {
      accessImageRegion(DxvkCmdBuffer::ExecBuffer, *dstImage, dstSubresource, dstOffset, extent, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);
      accessImageRegion(DxvkCmdBuffer::ExecBuffer, *srcImage, srcSubresource, srcOffset, extent, srcImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);
    }

    m_cmd->track(dstImage, DxvkAccess::Write);
    m_cmd->track(srcImage, DxvkAccess::Read);
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

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstImage->info().debugName;
      const char* srcName = srcImage->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Copy image (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

    auto dstSubresourceRange = vk::makeSubresourceRange(dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(srcSubresource);

    flushPendingAccesses(*dstImage, dstSubresourceRange, DxvkAccess::Write);
    flushPendingAccesses(*srcImage, srcSubresourceRange, DxvkAccess::Read);

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

    addImageLayoutTransition(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    addImageLayoutTransition(*dstImage, dstSubresourceRange, dstLayout,
      dstStages, dstAccess, doDiscard);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessImage(DxvkCmdBuffer::ExecBuffer, *srcImage, srcSubresourceRange,
      srcLayout, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, DxvkAccessOp::None);

    accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage, dstSubresourceRange,
      dstLayout, dstStages, dstAccess, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(dstImage, DxvkAccess::Write);
    m_cmd->track(srcImage, DxvkAccess::Read);
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
    const DxvkDeferredClear* clear = findDeferredClear(srcImage, vk::makeSubresourceRange(srcSubresource));

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
    if (dstImage->mipLevelExtent(dstSubresource.mipLevel, dstSubresource.aspectMask) != dstExtent)
      return false;

    auto view = dstImage->createView(viewInfo);

    prepareImage(dstImage, vk::makeSubresourceRange(dstSubresource));
    deferClear(view, srcSubresource.aspectMask, clear->clearValue);
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

    flushPendingAccesses(*buffer, offset, SparseMemoryPageSize * pageCount,
      ToBuffer ? DxvkAccess::Write : DxvkAccess::Read);

    if (pageTable->getBufferHandle()) {
      this->copySparseBufferPages<ToBuffer>(
        static_cast<DxvkBuffer*>(sparse.ptr()),
        pageCount, pages, buffer, offset);
    } else {
      this->copySparseImagePages<ToBuffer>(
        static_cast<DxvkImage*>(sparse.ptr()),
        pageCount, pages, buffer, offset);
    }

    accessBuffer(DxvkCmdBuffer::ExecBuffer, *buffer,
      offset, SparseMemoryPageSize * pageCount,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT,
      DxvkAccessOp::None);
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

    flushPendingAccesses(*sparse, 0, sparse->info().size,
      ToBuffer ? DxvkAccess::Read : DxvkAccess::Write);

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

    accessBuffer(DxvkCmdBuffer::ExecBuffer,
      *sparse, 0, sparse->info().size, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT,
      DxvkAccessOp::None);

    m_cmd->track(sparse, ToBuffer ? DxvkAccess::Read : DxvkAccess::Write);
    m_cmd->track(buffer, ToBuffer ? DxvkAccess::Write : DxvkAccess::Read);
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

    flushPendingAccesses(*sparse, sparseSubresources,
      ToBuffer ? DxvkAccess::Read : DxvkAccess::Write);

    VkImageLayout transferLayout = sparse->pickLayout(ToBuffer
      ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
      : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkAccessFlags transferAccess = ToBuffer
      ? VK_ACCESS_TRANSFER_READ_BIT
      : VK_ACCESS_TRANSFER_WRITE_BIT;

    addImageLayoutTransition(*sparse, sparseSubresources, transferLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, transferAccess, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessImage(DxvkCmdBuffer::ExecBuffer,
      *sparse, sparseSubresources, transferLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, transferAccess, DxvkAccessOp::None);

    m_cmd->track(sparse, ToBuffer ? DxvkAccess::Read : DxvkAccess::Write);
    m_cmd->track(buffer, ToBuffer ? DxvkAccess::Write : DxvkAccess::Read);
  }


  void DxvkContext::resolveImageHw(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    flushPendingAccesses(*dstImage, dstSubresourceRange, DxvkAccess::Write);
    flushPendingAccesses(*srcImage, srcSubresourceRange, DxvkAccess::Read);

    // We only support resolving to the entire image
    // area, so we might as well discard its contents
    VkImageLayout dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageLayout srcLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    addImageLayoutTransition(*dstImage, dstSubresourceRange, dstLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      dstImage->isFullSubresource(region.dstSubresource, region.extent));
    addImageLayoutTransition(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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
  
    accessImage(DxvkCmdBuffer::ExecBuffer,
      *dstImage, dstSubresourceRange, dstLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      VK_ACCESS_2_TRANSFER_WRITE_BIT, DxvkAccessOp::None);

    accessImage(DxvkCmdBuffer::ExecBuffer,
      *srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      VK_ACCESS_2_TRANSFER_READ_BIT, DxvkAccessOp::None);

    m_cmd->track(dstImage, DxvkAccess::Write);
    m_cmd->track(srcImage, DxvkAccess::Read);
  }


  void DxvkContext::resolveImageRp(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format,
          VkResolveModeFlagBits     mode,
          VkResolveModeFlagBits     stencilMode) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    bool isDepthStencil = (dstImage->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

    flushPendingAccesses(*dstImage, dstSubresourceRange, DxvkAccess::Write);
    flushPendingAccesses(*srcImage, srcSubresourceRange, DxvkAccess::Read);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstImage->info().debugName;
      const char* srcName = srcImage->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Resolve pass (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

    // Transition both images to usable layouts if necessary. For the source image
    // we can be fairly lenient when dealing with writable depth-stencil layouts.
    VkImageLayout writableLayout = isDepthStencil
      ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkImageLayout dstLayout = dstImage->pickLayout(writableLayout);
    VkImageLayout srcLayout = srcImage->info().layout;

    if (srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
     && srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      srcLayout = srcImage->pickLayout(writableLayout);

    VkPipelineStageFlags2 stages = isDepthStencil
      ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
      : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkAccessFlags2 srcAccess = isDepthStencil
      ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
      : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    VkAccessFlags2 dstAccess = isDepthStencil
      ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
      : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    addImageLayoutTransition(*srcImage, srcSubresourceRange, srcLayout, stages, srcAccess, false);
    addImageLayoutTransition(*dstImage, dstSubresourceRange, dstLayout, stages, dstAccess, true);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

    // Create a pair of views for the attachment resolve
    DxvkMetaResolveViews views(dstImage, region.dstSubresource,
      srcImage, region.srcSubresource, format);

    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = views.srcView->handle();
    attachment.imageLayout = srcLayout;
    attachment.resolveMode = mode;
    attachment.resolveImageView = views.dstView->handle();
    attachment.resolveImageLayout = dstLayout;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;

    VkRenderingAttachmentInfo stencilAttachment = attachment;
    stencilAttachment.resolveMode = stencilMode;

    VkExtent3D extent = dstImage->mipLevelExtent(region.dstSubresource.mipLevel);

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { extent.width, extent.height };
    renderingInfo.layerCount = region.dstSubresource.layerCount;

    if (isDepthStencil) {
      if (dstImage->formatInfo()->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
        renderingInfo.pDepthAttachment = &attachment;

      if (dstImage->formatInfo()->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
        renderingInfo.pStencilAttachment = &stencilAttachment;
    } else {
      renderingInfo.colorAttachmentCount = 1u;
      renderingInfo.pColorAttachments = &attachment;
    }

    m_cmd->cmdBeginRendering(&renderingInfo);
    m_cmd->cmdEndRendering();

    // Add barriers for the render pass resolve
    accessImage(DxvkCmdBuffer::ExecBuffer, *srcImage, srcSubresourceRange,
      srcLayout, stages, srcAccess, DxvkAccessOp::None);
    accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage, dstSubresourceRange,
      dstLayout, stages, dstAccess, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(dstImage, DxvkAccess::Write);
    m_cmd->track(srcImage, DxvkAccess::Read);
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

    flushPendingAccesses(*dstImage, dstSubresourceRange, DxvkAccess::Write);
    flushPendingAccesses(*srcImage, srcSubresourceRange, DxvkAccess::Read);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstImage->info().debugName;
      const char* srcName = srcImage->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Resolve (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

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

    // Check source image layout, and try to avoid transitions if we can
    VkImageLayout srcLayout = srcImage->info().layout;
    
    if (srcLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     && srcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
      srcLayout = (region.srcSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT & VK_IMAGE_ASPECT_COLOR_BIT)
        ? srcImage->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        : srcImage->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }

    addImageLayoutTransition(*dstImage, dstSubresourceRange,
      dstLayout, dstStages, dstAccess, doDiscard);
    addImageLayoutTransition(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);

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

    accessImage(DxvkCmdBuffer::ExecBuffer, *srcImage, srcSubresourceRange,
      srcLayout, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, DxvkAccessOp::None);

    accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage, dstSubresourceRange,
      dstLayout, dstStages, dstAccess, DxvkAccessOp::None);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(dstImage, DxvkAccess::Write);
    m_cmd->track(srcImage, DxvkAccess::Read);
  }


  bool DxvkContext::resolveImageClear(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format) {
    // If the destination image is only partially written, ignore
    if (dstImage->mipLevelExtent(region.dstSubresource.mipLevel, region.dstSubresource.aspectMask) != region.extent)
      return false;

    // Find a pending clear that overlaps with the source image
    const DxvkDeferredClear* clear = findDeferredClear(srcImage, vk::makeSubresourceRange(region.srcSubresource));

    if (!clear)
      return false;

    // The clear format must match the resolve format, or
    // otherwise we cannot reuse the clear value
    if (clear->imageView->info().format != format)
      return false;

    // Ensure that we can actually clear the image as intended. We can be
    // aggressive here since we know the destination image has a format
    // that can be used for rendering.
    bool isDepthStencil = region.dstSubresource.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    DxvkImageUsageInfo usage = { };
    usage.usage = isDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage.viewFormatCount = 1;
    usage.viewFormats = &format;

    if (!ensureImageCompatibility(dstImage, usage))
      return false;

    // End current render pass and prepare the destination image
    spillRenderPass(true);
    prepareImage(dstImage, vk::makeSubresourceRange(region.dstSubresource));

    // Create an image view that we can use to perform the clear
    DxvkImageViewKey key = { };
    key.viewType = VK_IMAGE_VIEW_TYPE_2D;
    key.usage = usage.usage;
    key.format = format;
    key.aspects = region.dstSubresource.aspectMask;
    key.layerIndex = region.dstSubresource.baseArrayLayer;
    key.layerCount = region.dstSubresource.layerCount;
    key.mipIndex = region.dstSubresource.mipLevel;
    key.mipCount = 1u;

    if (isDepthStencil)
      key.aspects = dstImage->formatInfo()->aspectMask;

    deferClear(dstImage->createView(key), region.dstSubresource.aspectMask, clear->clearValue);
    return true;
  }


  bool DxvkContext::resolveImageInline(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format,
          VkResolveModeFlagBits     depthMode,
          VkResolveModeFlagBits     stencilMode) {
    // Ignore any non-2D images due to the added complexity, and the
    // source image is going to be a multisampled 2D image anyway.
    if (dstImage->info().type != VK_IMAGE_TYPE_2D)
      return false;

    // Check if we can implement the resolve as a clear first
    VkImageResolve clearRegion = region;

    if (!depthMode) {
      clearRegion.dstSubresource.aspectMask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
      clearRegion.srcSubresource.aspectMask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (!stencilMode) {
      clearRegion.dstSubresource.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;
      clearRegion.srcSubresource.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (resolveImageClear(dstImage, srcImage, clearRegion, format))
      return true;

    // Need an active render pass with secondary command buffers to
    // fold resolve operations into it
    if (!m_flags.test(DxvkContextFlag::GpRenderPassSecondaryCmd))
      return false;

    // Otherwise, if any clears are queued up for the source image,
    // that clear operation needs to be performed first.
    if (findOverlappingDeferredClear(srcImage, vk::makeSubresourceRange(region.srcSubresource)))
      flushClearsInline();

    // Check whether we're dealing with a color or depth attachment, one
    // of those flags needs to be set for resolve to even make sense
    VkImageUsageFlags usage = srcImage->info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    if (!usage)
      return false;

    // We can't support partial resolves here
    if (region.srcOffset.x || region.srcOffset.y || region.srcOffset.z
     || region.dstOffset.x || region.dstOffset.y || region.dstOffset.z
     || region.extent != dstImage->mipLevelExtent(region.dstSubresource.mipLevel, region.dstSubresource.aspectMask)
     || region.extent.width != m_state.om.renderingInfo.rendering.renderArea.extent.width
     || region.extent.height != m_state.om.renderingInfo.rendering.renderArea.extent.height)
      return false;

    // The array layer we're dealing with relative to the source image,
    // if layered resolves are split across multiple resolve calls.
    uint32_t relativeLayer = 0u;

    DxvkImageViewKey dstKey = { };
    dstKey.usage = usage;
    dstKey.aspects = region.dstSubresource.aspectMask;
    dstKey.mipIndex = region.dstSubresource.mipLevel;
    dstKey.mipCount = 1u;

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      dstKey.aspects = lookupFormatInfo(format)->aspectMask;

    // Need the source image to be bound with a superset of the subresources that
    // we're resolving. The resolve format is allowed to differ in sRGB-ness only.
    int32_t attachmentIndex = -1;

    for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
      const auto& attachment = m_state.om.framebufferInfo.getAttachment(i);

      if (attachment.view->image() == srcImage) {
        VkImageSubresourceRange subresources = attachment.view->imageSubresources();

        if ((subresources.aspectMask & region.srcSubresource.aspectMask)
         && (subresources.baseMipLevel == region.srcSubresource.mipLevel)
         && (subresources.levelCount == 1u)
         && (subresources.baseArrayLayer <= region.srcSubresource.baseArrayLayer)
         && (subresources.baseArrayLayer + subresources.layerCount >= region.srcSubresource.baseArrayLayer + region.srcSubresource.layerCount)
         && formatsAreResolveCompatible(format, attachment.view->info().format)) {
          relativeLayer = region.srcSubresource.baseArrayLayer - subresources.baseArrayLayer;

          dstKey.viewType = attachment.view->info().viewType;
          dstKey.format = attachment.view->info().format;

          attachmentIndex = i;
          break;
        }
      }
    }

    if (attachmentIndex < 0)
      return false;

    // Need to check if the source image is actually currently bound for rendering
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      uint32_t index = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

      if (index >= m_state.om.renderingInfo.rendering.colorAttachmentCount
       || !m_state.om.renderingInfo.color[index].imageView)
        return false;
    } else {
      if ((!m_state.om.renderingInfo.rendering.pDepthAttachment || !m_state.om.renderingInfo.depth.imageView)
       && (!m_state.om.renderingInfo.rendering.pStencilAttachment || !m_state.om.renderingInfo.stencil.imageView))
        return false;
    }

    // If we can't properly map bound array layers to the detination image,
    // skip. This could e.g. be the case if we're trying to resolve relative
    // layer 1 into destination layer 0.
    if (relativeLayer > region.dstSubresource.baseArrayLayer)
      return false;

    dstKey.layerIndex = region.dstSubresource.baseArrayLayer - relativeLayer;
    dstKey.layerCount = m_state.om.renderingInfo.rendering.layerCount;

    if (dstKey.layerIndex + dstKey.layerCount > dstImage->info().numLayers)
      return false;

    // Create view to bind as the attachment
    Rc<DxvkImageView> dstView = dstImage->createView(dstKey);

    // Detect duplicate resolves, and error out if we
    // have already set a different resolve attachment
    auto& resolve = m_deferredResolves.at(attachmentIndex);

    if (resolve.imageView && resolve.imageView != dstView)
      return false;

    resolve.imageView = dstView;
    resolve.layerMask |= 1u << relativeLayer;
    resolve.depthMode = depthMode;
    resolve.stencilMode = stencilMode;

    // Ensure resolves get flushed before the next draw
    m_flags.set(DxvkContextFlag::GpRenderPassNeedsFlush);
    return true;
  }


  void DxvkContext::uploadImageFb(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset,
          VkDeviceSize              subresourceAlignment,
          VkFormat                  format) {
    if (!format)
      format = image->info().format;

    for (uint32_t m = 0; m < image->info().mipLevels; m++) {
      VkExtent3D mipExtent = image->mipLevelExtent(m);

      VkDeviceSize mipSize = util::computeImageDataSize(
        format, mipExtent, image->formatInfo()->aspectMask);

      for (uint32_t l = 0; l < image->info().numLayers; l++) {
        VkImageSubresourceLayers layers = { };
        layers.aspectMask = image->formatInfo()->aspectMask;
        layers.mipLevel = m;
        layers.baseArrayLayer = l;
        layers.layerCount = 1;

        copyBufferToImageFb(image, layers,
          VkOffset3D { 0, 0, 0 }, mipExtent,
          source, sourceOffset, 0, 0, format);

        sourceOffset += align(mipSize, subresourceAlignment);
      }
    }
  }


  void DxvkContext::uploadImageHw(
    const Rc<DxvkImage>&            image,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset,
          VkDeviceSize              subresourceAlignment) {
    // Initialize all subresources of the image at once
    VkImageLayout transferLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    addImageInitTransition(*image, image->getAvailableSubresources(),
      transferLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    flushImageLayoutTransitions(DxvkCmdBuffer::SdmaBuffer);

    // Copy image data, one mip at a time
    VkDeviceSize dataOffset = sourceOffset;

    for (uint32_t m = 0; m < image->info().mipLevels; m++) {
      VkExtent3D mipExtent = image->mipLevelExtent(m);

      VkDeviceSize mipSize = util::computeImageDataSize(
        image->info().format, mipExtent, image->formatInfo()->aspectMask);

      for (uint32_t l = 0; l < image->info().numLayers; l++) {
        VkImageSubresourceLayers layers = { };
        layers.aspectMask = image->formatInfo()->aspectMask;
        layers.mipLevel = m;
        layers.baseArrayLayer = l;
        layers.layerCount = 1;

        copyImageBufferData<true>(DxvkCmdBuffer::SdmaBuffer,
          image, layers, VkOffset3D { 0, 0, 0 }, mipExtent, transferLayout,
          source->getSliceHandle(dataOffset, mipSize), 0, 0);

        dataOffset += align(mipSize, subresourceAlignment);
      }
    }

    accessImageTransfer(*image, image->getAvailableSubresources(), transferLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    m_cmd->track(source, DxvkAccess::Read);
    m_cmd->track(image, DxvkAccess::Write);
  }


  void DxvkContext::startRenderPass() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound)) {
      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl);

      this->applyRenderTargetLoadLayouts();
      this->flushClears(true);

      // Make sure all graphics state gets reapplied on the next draw
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

      m_flags.set(
        DxvkContextFlag::GpRenderPassBound,
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

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        beginRenderPassDebugRegion();

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
      m_flags.clr(DxvkContextFlag::GpRenderPassBound,
                  DxvkContextFlag::GpRenderPassSideEffects,
                  DxvkContextFlag::GpRenderPassNeedsFlush);

      this->pauseTransformFeedback();

      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_OCCLUSION);
      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_PIPELINE_STATISTICS);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl);

      this->renderPassUnbindFramebuffer();

      if (suspend)
        m_flags.set(DxvkContextFlag::GpRenderPassSuspended);
      else
        this->transitionRenderTargetLayouts(false);

      if (m_renderPassBarrierSrc.stages) {
        accessMemory(DxvkCmdBuffer::ExecBuffer,
          m_renderPassBarrierSrc.stages, m_renderPassBarrierSrc.access,
          m_renderPassBarrierDst.stages, m_renderPassBarrierDst.access);

        m_renderPassBarrierSrc = DxvkGlobalPipelineBarrier();
      }

      flushBarriers();
      flushResolves();

      // Need to process pending clears after resolves
      preparePostRenderPassClears();

      if (!suspend)
        flushClears(false);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalRenderPass);
    } else if (!suspend) {
      // We may end a previously suspended render pass
      if (m_flags.test(DxvkContextFlag::GpRenderPassSuspended)) {
        m_flags.clr(DxvkContextFlag::GpRenderPassSuspended);
        this->transitionRenderTargetLayouts(false);
        flushBarriers();
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

      flushPendingAccesses(*attachment.view->image(),
        attachment.view->imageSubresources(), DxvkAccess::Write);
    }

    // Transition all images to the render layout as necessary
    const auto& depthAttachment = framebufferInfo.getDepthTarget();

    if (depthAttachment.layout != ops.depthOps.loadLayout
     && depthAttachment.view != nullptr) {
      VkImageAspectFlags depthAspects = depthAttachment.view->info().aspects;

      VkPipelineStageFlags2 depthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      VkAccessFlags2 depthAccess = VK_ACCESS_2_NONE;

      if (((depthAspects & VK_IMAGE_ASPECT_DEPTH_BIT) && ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_LOAD)
       || ((depthAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_LOAD))
        depthAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

      if (((depthAspects & VK_IMAGE_ASPECT_DEPTH_BIT) && ops.depthOps.loadOpD != VK_ATTACHMENT_LOAD_OP_LOAD)
       || ((depthAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && ops.depthOps.loadOpS != VK_ATTACHMENT_LOAD_OP_LOAD)
       || (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL))
        depthAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        depthStages |= m_device->getShaderPipelineStages();
        depthAccess |= VK_ACCESS_2_SHADER_READ_BIT;
      }

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *depthAttachment.view->image(),
        depthAttachment.view->imageSubresources(),
        ops.depthOps.loadLayout,
        depthStages, 0,
        depthAttachment.layout,
        depthStages, depthAccess, DxvkAccessOp::None);
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorAttachment = framebufferInfo.getColorTarget(i);

      if (colorAttachment.layout != ops.colorOps[i].loadLayout
       && colorAttachment.view != nullptr) {
        VkAccessFlags2 colorAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        if (ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
          colorAccess |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

        accessImage(DxvkCmdBuffer::ExecBuffer,
          *colorAttachment.view->image(),
          colorAttachment.view->imageSubresources(),
          ops.colorOps[i].loadLayout,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
          colorAttachment.layout,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          colorAccess, DxvkAccessOp::None);
      }
    }

    // Unconditionally emit barriers here. We need to do this
    // even if there are no layout transitions, since we don't
    // track resource usage during render passes.
    flushBarriers();
  }


  void DxvkContext::renderPassEmitPostBarriers(
    const DxvkFramebufferInfo&  framebufferInfo,
    const DxvkRenderPassOps&    ops) {
    const auto& depthAttachment = framebufferInfo.getDepthTarget();

    if (depthAttachment.view != nullptr) {
      VkAccessFlags2 srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

      if (depthAttachment.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        srcAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *depthAttachment.view->image(),
        depthAttachment.view->imageSubresources(),
        depthAttachment.layout,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        srcAccess, ops.depthOps.storeLayout,
        depthAttachment.view->image()->info().stages,
        depthAttachment.view->image()->info().access,
        DxvkAccessOp::None);
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorAttachment = framebufferInfo.getColorTarget(i);

      if (colorAttachment.view != nullptr) {
        accessImage(DxvkCmdBuffer::ExecBuffer,
          *colorAttachment.view->image(),
          colorAttachment.view->imageSubresources(),
          colorAttachment.layout,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
          ops.colorOps[i].storeLayout,
          colorAttachment.view->image()->info().stages,
          colorAttachment.view->image()->info().access,
          DxvkAccessOp::None);
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

    m_state.om.attachmentMask.clear();

    VkCommandBufferInheritanceRenderingInfo renderingInheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO };
    VkCommandBufferInheritanceInfo inheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, &renderingInheritance };

    uint32_t colorInfoCount = 0;
    uint32_t lateClearCount = 0;

    std::array<VkFormat, MaxNumRenderTargets> colorFormats = { };
    std::array<VkClearAttachment, MaxNumRenderTargets> lateClears = { };

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorTarget = framebufferInfo.getColorTarget(i);

      auto& colorInfo = m_state.om.renderingInfo.color[i];
      colorInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

      if (colorTarget.view != nullptr) {
        colorFormats[i] = colorTarget.view->info().format;

        colorInfo.imageView = colorTarget.view->handle();
        colorInfo.imageLayout = colorTarget.layout;
        colorInfo.loadOp = ops.colorOps[i].loadOp;
        colorInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        renderingInheritance.rasterizationSamples = colorTarget.view->image()->info().sampleCount;

        if (ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
          colorInfo.clearValue.color = ops.colorOps[i].clearValue;

          if (m_device->perfHints().renderPassClearFormatBug
           && colorTarget.view->info().format != colorTarget.view->image()->info().format) {
            colorInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

            auto& clear = lateClears[lateClearCount++];
            clear.colorAttachment = i;
            clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clear.clearValue.color = ops.colorOps[i].clearValue;
          }
        }

        colorInfoCount = i + 1;
      }
    }

    VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags depthStencilAspects = 0;
    VkImageAspectFlags depthStencilWritable = 0;

    auto& depthInfo = m_state.om.renderingInfo.depth;
    depthInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

    const auto& depthTarget = framebufferInfo.getDepthTarget();

    if (depthTarget.view) {
      depthStencilFormat = depthTarget.view->info().format;
      depthStencilAspects = depthTarget.view->info().aspects;
      depthStencilWritable = vk::getWritableAspectsForLayout(depthTarget.layout);

      if (!m_device->properties().khrMaintenance7.separateDepthStencilAttachmentAccess && depthStencilWritable)
        depthStencilWritable = depthStencilAspects;

      depthInfo.imageView = depthTarget.view->handle();
      depthInfo.imageLayout = depthTarget.layout;
      depthInfo.loadOp = ops.depthOps.loadOpD;
      depthInfo.storeOp = (depthStencilWritable & VK_IMAGE_ASPECT_DEPTH_BIT)
        ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_NONE;

      if (ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        depthInfo.clearValue.depthStencil.depth = ops.depthOps.clearValue.depth;
        depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      }

      renderingInheritance.rasterizationSamples = depthTarget.view->image()->info().sampleCount;
    }

    auto& stencilInfo = m_state.om.renderingInfo.stencil;
    stencilInfo = depthInfo;

    if (depthTarget.view) {
      stencilInfo.loadOp = ops.depthOps.loadOpS;
      stencilInfo.storeOp = (depthStencilWritable & VK_IMAGE_ASPECT_STENCIL_BIT)
        ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_NONE;

      if (ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        stencilInfo.clearValue.depthStencil.stencil = ops.depthOps.clearValue.stencil;
        stencilInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      }
    }

    auto& renderingInfo = m_state.om.renderingInfo.rendering;
    renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.offset = VkOffset2D { 0, 0 };
    renderingInfo.renderArea.extent = VkExtent2D { fbSize.width, fbSize.height };
    renderingInfo.layerCount = fbSize.layers;

    if (colorInfoCount) {
      renderingInfo.colorAttachmentCount = colorInfoCount;
      renderingInfo.pColorAttachments = m_state.om.renderingInfo.color.data();
      renderingInheritance.colorAttachmentCount = colorInfoCount;
      renderingInheritance.pColorAttachmentFormats = colorFormats.data();
    }

    if (depthStencilAspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
      renderingInfo.pDepthAttachment = &depthInfo;
      renderingInheritance.depthAttachmentFormat = depthStencilFormat;
    }

    if (depthStencilAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      renderingInfo.pStencilAttachment = &stencilInfo;
      renderingInheritance.stencilAttachmentFormat = depthStencilFormat;
    }

    // On drivers that don't natively support secondary command buffers, only use
    // them to enable MSAA resolve attachments. Also ignore render passes with only
    // one color attachment here since those tend to only have a small number of
    // draws and we are almost certainly going to use the output anyway.
    bool useSecondaryCmdBuffer = !m_device->perfHints().preferPrimaryCmdBufs
      && renderingInheritance.rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

    if (m_device->perfHints().preferRenderPassOps) {
      useSecondaryCmdBuffer = renderingInheritance.rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

      if (!m_device->perfHints().preferPrimaryCmdBufs)
        useSecondaryCmdBuffer |= depthStencilAspects || colorInfoCount > 1u;
    }

    if (useSecondaryCmdBuffer) {
      // Begin secondary command buffer on tiling GPUs so that subsequent
      // resolve, discard and clear commands can modify render pass ops.
      m_flags.set(DxvkContextFlag::GpRenderPassSecondaryCmd);

      renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

      m_cmd->beginSecondaryCommandBuffer(inheritance);
    } else {
      // Begin rendering right away on regular GPUs
      m_cmd->cmdBeginRendering(&renderingInfo);
    }

    if (lateClearCount) {
      VkClearRect clearRect = { };
      clearRect.rect.extent.width   = fbSize.width;
      clearRect.rect.extent.height  = fbSize.height;
      clearRect.layerCount          = fbSize.layers;

      m_cmd->cmdClearAttachments(lateClearCount, lateClears.data(), 1, &clearRect);
    }

    for (uint32_t i = 0; i < framebufferInfo.numAttachments(); i++) {
      const auto& attachment = framebufferInfo.getAttachment(i);
      m_cmd->track(attachment.view->image(), DxvkAccess::Write);

      if (attachment.view->isMultisampled()) {
        VkImageSubresourceRange subresources = attachment.view->imageSubresources();

        if (subresources.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
          subresources.aspectMask = vk::getWritableAspectsForLayout(attachment.layout);

        m_implicitResolves.invalidate(*attachment.view->image(), subresources);
      }
    }

    m_cmd->addStatCtr(DxvkStatCounter::CmdRenderPassCount, 1u);
  }
  
  
  void DxvkContext::renderPassUnbindFramebuffer() {
    if (m_flags.test(DxvkContextFlag::GpRenderPassSecondaryCmd)) {
      m_flags.clr(DxvkContextFlag::GpRenderPassSecondaryCmd);
      VkCommandBuffer cmdBuffer = m_cmd->endSecondaryCommandBuffer();

      // Record scoped rendering commands with potentially
      // modified store or resolve ops here
      flushRenderPassResolves();
      flushRenderPassDiscards();

      // Need information about clears, discards and resolve
      // to set up the final load and store ops for the pass
      finalizeLoadStoreOps();

      auto& renderingInfo = m_state.om.renderingInfo.rendering;
      m_cmd->cmdBeginRendering(&renderingInfo);
      m_cmd->cmdExecuteCommands(1, &cmdBuffer);
    }

    // End actual rendering command
    m_cmd->cmdEndRendering();

    // If there are pending layout transitions, execute them immediately
    // since the backend expects images to be in the store layout after
    // a render pass instance. This is expected to be rare.
    if (m_execBarriers.hasLayoutTransitions())
      flushBarriers();
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

        if (physSlice.handle) {
          // Just in case someone is mad enough to write to a
          // transform feedback buffer from a shader as well
          m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

          accessBuffer(DxvkCmdBuffer::ExecBuffer, m_state.xfb.activeCounters[i],
            VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,
            VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
            VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
            DxvkAccessOp::None);

          m_cmd->track(m_state.xfb.activeCounters[i].buffer(), DxvkAccess::Write);
        }
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

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dca2, newPipeline->debugName()));
    }

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

    m_state.gp.flags = newFlags;

    if (diffFlags.test(DxvkGraphicsPipelineFlag::HasSampleMaskExport))
      m_flags.set(DxvkContextFlag::GpDirtyMultisampleState);

    m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

    if (newPipeline->getBindings()->layout().getPushConstantRange(true).size)
      m_flags.set(DxvkContextFlag::DirtyPushConstants);

    m_flags.clr(DxvkContextFlag::GpDirtyPipeline);
    return true;
  }
  
  
  bool DxvkContext::updateGraphicsPipelineState() {
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

    if (unlikely(!pipelineInfo.handle))
      return false;

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineInfo.handle);

    // Update attachment usage info based on the pipeline state
    m_state.om.attachmentMask.merge(pipelineInfo.attachments);

    // For pipelines created from graphics pipeline libraries, we need to
    // apply a bunch of dynamic state that is otherwise static or unused
    if (pipelineInfo.type == DxvkGraphicsPipelineType::BasePipeline) {
      m_flags.set(
        DxvkContextFlag::GpDynamicDepthStencilState,
        DxvkContextFlag::GpDynamicDepthBias,
        DxvkContextFlag::GpDynamicStencilRef,
        DxvkContextFlag::GpIndependentSets);

      if (m_device->features().core.features.depthBounds)
        m_flags.set(DxvkContextFlag::GpDynamicDepthBounds);

      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
       && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask) {
        m_flags.set(m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasSampleRateShading)
          ? DxvkContextFlag::GpDynamicMultisampleState
          : DxvkContextFlag::GpDirtyMultisampleState);
       }
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
    DxvkGlobalPipelineBarrier srcBarrier = m_state.gp.pipeline->getGlobalBarrier(m_state.gp.state);
    m_renderPassBarrierSrc.stages |= srcBarrier.stages;
    m_renderPassBarrierSrc.access |= srcBarrier.access;

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      uint32_t color = getGraphicsPipelineDebugColor();

      m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(color, m_state.gp.pipeline->debugName()));
    }

    m_flags.clr(DxvkContextFlag::GpDirtyPipelineState);
    return true;
  }


  uint32_t DxvkContext::getGraphicsPipelineDebugColor() const {
    if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasStorageDescriptors))
      return 0xf0a2dc;

    if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback))
      return 0xa2f0dc;

    return 0xa2dcf0;
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

              m_cmd->track(res.sampler);
            } else {
              descriptorInfo.image.sampler = m_common->dummyResources().samplerHandle();
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
            const auto& res = m_rc[binding.resourceBinding];

            VkImageView viewHandle = VK_NULL_HANDLE;

            if (res.imageView != nullptr)
              viewHandle = res.imageView->handle(binding.viewType);

            if (viewHandle) {
              if (likely(!res.imageView->isMultisampled() || binding.isMultisampled)) {
                descriptorInfo.image.sampler = VK_NULL_HANDLE;
                descriptorInfo.image.imageView = viewHandle;
                descriptorInfo.image.imageLayout = res.imageView->defaultLayout();

                if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || unlikely(res.imageView->hasGfxStores()))
                  accessImage(DxvkCmdBuffer::ExecBuffer, *res.imageView, util::pipelineStages(binding.stage), binding.access, DxvkAccessOp::None);

                m_cmd->track(res.imageView->image(), DxvkAccess::Read);
              } else {
                auto view = m_implicitResolves.getResolveView(*res.imageView, m_trackingId);

                descriptorInfo.image.sampler = VK_NULL_HANDLE;
                descriptorInfo.image.imageView = view->handle(binding.viewType);
                descriptorInfo.image.imageLayout = view->defaultLayout();

                m_cmd->track(view->image(), DxvkAccess::Read);
              }
            } else {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            const auto& res = m_rc[binding.resourceBinding];

            VkImageView viewHandle = VK_NULL_HANDLE;

            if (res.imageView != nullptr)
              viewHandle = res.imageView->handle(binding.viewType);

            if (viewHandle) {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = viewHandle;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

              if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || res.imageView->hasGfxStores())
                accessImage(DxvkCmdBuffer::ExecBuffer, *res.imageView, util::pipelineStages(binding.stage), binding.access, binding.accessOp);

              m_cmd->track(res.imageView->image(), (binding.access & vk::AccessWriteMask)
                ? DxvkAccess::Write : DxvkAccess::Read);
            } else {
              descriptorInfo.image.sampler = VK_NULL_HANDLE;
              descriptorInfo.image.imageView = VK_NULL_HANDLE;
              descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            const auto& res = m_rc[binding.resourceBinding];

            VkImageView viewHandle = VK_NULL_HANDLE;

            if (res.imageView != nullptr && res.sampler != nullptr)
              viewHandle = res.imageView->handle(binding.viewType);

            if (viewHandle) {
              if (likely(!res.imageView->isMultisampled() || binding.isMultisampled)) {
                descriptorInfo.image.sampler = res.sampler->handle();
                descriptorInfo.image.imageView = viewHandle;
                descriptorInfo.image.imageLayout = res.imageView->defaultLayout();

                if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || unlikely(res.imageView->hasGfxStores()))
                  accessImage(DxvkCmdBuffer::ExecBuffer, *res.imageView, util::pipelineStages(binding.stage), binding.access, DxvkAccessOp::None);

                m_cmd->track(res.imageView->image(), DxvkAccess::Read);
                m_cmd->track(res.sampler);
              } else {
                auto view = m_implicitResolves.getResolveView(*res.imageView, m_trackingId);

                descriptorInfo.image.sampler = res.sampler->handle();
                descriptorInfo.image.imageView = view->handle(binding.viewType);
                descriptorInfo.image.imageLayout = view->defaultLayout();

                m_cmd->track(view->image(), DxvkAccess::Read);
                m_cmd->track(res.sampler);
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

              if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || unlikely(res.bufferView->buffer()->hasGfxStores()))
                accessBuffer(DxvkCmdBuffer::ExecBuffer, *res.bufferView, util::pipelineStages(binding.stage), binding.access, DxvkAccessOp::None);

              m_cmd->track(res.bufferView->buffer(), DxvkAccess::Read);
            } else {
              descriptorInfo.texelBuffer = VK_NULL_HANDLE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferView != nullptr) {
              descriptorInfo.texelBuffer = res.bufferView->handle();

              if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || res.bufferView->buffer()->hasGfxStores())
                accessBuffer(DxvkCmdBuffer::ExecBuffer, *res.bufferView, util::pipelineStages(binding.stage), binding.access, binding.accessOp);

              m_cmd->track(res.bufferView->buffer(), (binding.access & vk::AccessWriteMask)
                ? DxvkAccess::Write : DxvkAccess::Read);
            } else {
              descriptorInfo.texelBuffer = VK_NULL_HANDLE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
            const auto& res = m_rc[binding.resourceBinding];

            if (res.bufferSlice.length()) {
              descriptorInfo = res.bufferSlice.getDescriptor();

              if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || unlikely(res.bufferSlice.buffer()->hasGfxStores()))
                accessBuffer(DxvkCmdBuffer::ExecBuffer, res.bufferSlice, util::pipelineStages(binding.stage), binding.access, DxvkAccessOp::None);

              m_cmd->track(res.bufferSlice.buffer(), DxvkAccess::Read);
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

              if (BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE || unlikely(res.bufferSlice.buffer()->hasGfxStores()))
                accessBuffer(DxvkCmdBuffer::ExecBuffer, res.bufferSlice, util::pipelineStages(binding.stage), binding.access, binding.accessOp);

              m_cmd->track(res.bufferSlice.buffer(), (binding.access & vk::AccessWriteMask)
                ? DxvkAccess::Write : DxvkAccess::Read);
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
    } else if (m_flags.test(DxvkContextFlag::GpRenderPassNeedsFlush)) {
      // End render pass to flush pending resolves
      this->spillRenderPass(true);
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
      accessImage(DxvkCmdBuffer::ExecBuffer,
        *attachment.view->image(),
        attachment.view->imageSubresources(), oldLayout,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, DxvkAccessOp::None);

      m_cmd->track(attachment.view->image(), DxvkAccess::Write);
    }
  }


  void DxvkContext::transitionDepthAttachment(
    const DxvkAttachment&         attachment,
          VkImageLayout           oldLayout) {
    if (oldLayout != attachment.view->image()->info().layout) {
      VkAccessFlags2 access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

      if (oldLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      accessImage(DxvkCmdBuffer::ExecBuffer,
        *attachment.view->image(),
        attachment.view->imageSubresources(), oldLayout,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        access, DxvkAccessOp::None);

      m_cmd->track(attachment.view->image(), DxvkAccess::Write);
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

    // Flush clears if there are any that affect the image. We need
    // to check all subresources here, not just the ones to be used.
    if (flushClears && findOverlappingDeferredClear(image, image->getAvailableSubresources()))
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
         && (is3D || vk::checkSubresourceRangeOverlap(attachment.view->imageSubresources(), subresources))) {
          this->transitionColorAttachment(attachment, m_rtLayouts.color[i]);
          m_rtLayouts.color[i] = image->info().layout;
        }
      }
    } else {
      const DxvkAttachment& attachment = m_state.om.framebufferInfo.getDepthTarget();

      if (attachment.view != nullptr && attachment.view->image() == image
       && (is3D || vk::checkSubresourceRangeOverlap(attachment.view->imageSubresources(), subresources))) {
        this->transitionDepthAttachment(attachment, m_rtLayouts.depth);
        m_rtLayouts.depth = image->info().layout;
      }
    }
  }


  DxvkDeferredClear* DxvkContext::findDeferredClear(
    const Rc<DxvkImage>&          image,
    const VkImageSubresourceRange& subresources) {
    for (auto& entry : m_deferredClears) {
      if ((entry.imageView->image() == image.ptr()) && ((subresources.aspectMask & entry.clearAspects) == subresources.aspectMask)
       && (vk::checkSubresourceRangeSuperset(entry.imageView->imageSubresources(), subresources)))
        return &entry;
    }

    return nullptr;
  }


  DxvkDeferredClear* DxvkContext::findOverlappingDeferredClear(
    const Rc<DxvkImage>&          image,
    const VkImageSubresourceRange& subresources) {
    for (auto& entry : m_deferredClears) {
      if ((entry.imageView->image() == image.ptr())
       && ((entry.clearAspects | entry.discardAspects) | subresources.aspectMask)
       && (vk::checkSubresourceRangeOverlap(entry.imageView->imageSubresources(), subresources)))
        return &entry;
    }

    return nullptr;
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

    if (unlikely(m_state.vi.indexBuffer.buffer()->hasGfxStores())) {
      accessBuffer(DxvkCmdBuffer::ExecBuffer, m_state.vi.indexBuffer,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, DxvkAccessOp::None);
    }

    m_renderPassBarrierSrc.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    m_renderPassBarrierSrc.access |= VK_ACCESS_INDEX_READ_BIT;

    m_cmd->track(m_state.vi.indexBuffer.buffer(), DxvkAccess::Read);
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

        if (unlikely(m_state.vi.vertexBuffers[binding].buffer()->hasGfxStores())) {
          accessBuffer(DxvkCmdBuffer::ExecBuffer, m_state.vi.vertexBuffers[binding],
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, DxvkAccessOp::None);
        }

        m_cmd->track(m_state.vi.vertexBuffers[binding].buffer(), DxvkAccess::Read);
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

      if (!physSlice.handle)
        xfbBuffers[i] = m_common->dummyResources().bufferHandle();

      if (physSlice.handle) {
        Rc<DxvkBuffer> buffer = m_state.xfb.buffers[i].buffer();
        buffer->setXfbVertexStride(gsInfo.xfbStrides[i]);

        accessBuffer(DxvkCmdBuffer::ExecBuffer, m_state.xfb.buffers[i],
          VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,
          VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, DxvkAccessOp::None);

        m_cmd->track(std::move(buffer), DxvkAccess::Write);
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

      // Clamp scissor against rendering area. Not doing so is technically
      // out of spec, even if this doesn't get validated. This also solves
      // problems with potentially invalid scissor rects.
      std::array<VkRect2D, DxvkLimits::MaxNumViewports> clampedScissors;
      DxvkFramebufferSize renderSize = m_state.om.framebufferInfo.size();

      for (uint32_t i = 0; i < m_state.vp.viewportCount; i++) {
        const auto& scissor = m_state.vp.scissorRects[i];

        clampedScissors[i].offset = VkOffset2D {
          std::clamp<int32_t>(scissor.offset.x, 0, renderSize.width),
          std::clamp<int32_t>(scissor.offset.y, 0, renderSize.height) };
        clampedScissors[i].extent = VkExtent2D {
          uint32_t(std::clamp<int32_t>(scissor.offset.x + scissor.extent.width,  clampedScissors[i].offset.x, renderSize.width)  - clampedScissors[i].offset.x),
          uint32_t(std::clamp<int32_t>(scissor.offset.y + scissor.extent.height, clampedScissors[i].offset.y, renderSize.height) - clampedScissors[i].offset.y) };
      }

      m_cmd->cmdSetViewport(m_state.vp.viewportCount, m_state.vp.viewports.data());
      m_cmd->cmdSetScissor(m_state.vp.viewportCount, clampedScissors.data());
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
  

  template<bool Resolve>
  bool DxvkContext::commitComputeState() {
    this->spillRenderPass(false);

    if (m_flags.any(
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::CpDirtySpecConstants)) {
      if (unlikely(!this->updateComputePipelineState()))
        return false;
    }

    if (this->checkComputeHazards()) {
      this->flushBarriers();

      // Dirty descriptors if this hasn't happened yet for
      // whatever reason in order to re-emit barriers
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_COMPUTE_BIT);
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      this->beginBarrierControlDebugRegion<VK_PIPELINE_BIND_POINT_COMPUTE>();

    if (m_descriptorState.hasDirtyComputeSets()) {
      this->updateComputeShaderResources();

      if (unlikely(Resolve && m_implicitResolves.hasPendingResolves())) {
        this->flushImplicitResolves();
        return this->commitComputeState<false>();
      }
    }

    if (m_flags.test(DxvkContextFlag::DirtyPushConstants))
      this->updatePushConstants<VK_PIPELINE_BIND_POINT_COMPUTE>();

    return true;
  }
  
  
  template<bool Indexed, bool Indirect, bool Resolve>
  bool DxvkContext::commitGraphicsState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyPipeline)) {
      if (unlikely(!this->updateGraphicsPipeline()))
        return false;
    }

    // End render pass if there are pending resolves
    if (m_flags.any(DxvkContextFlag::GpDirtyFramebuffer,
                    DxvkContextFlag::GpRenderPassNeedsFlush))
      this->updateFramebuffer();

    if (m_flags.test(DxvkContextFlag::GpXfbActive)) {
      // If transform feedback is active and there is a chance that we might
      // need to rebind the pipeline, we need to end transform feedback and
      // issue a barrier. End the render pass to do that. Ignore dirty vertex
      // buffers here since non-dynamic vertex strides are such an extreme
      // edge case that it's likely irrelevant in practice.
      if (m_flags.any(DxvkContextFlag::GpDirtyPipelineState,
                      DxvkContextFlag::GpDirtySpecConstants,
                      DxvkContextFlag::GpDirtyXfbBuffers))
        this->spillRenderPass(true);
    }

    if (m_flags.test(DxvkContextFlag::GpRenderPassSideEffects)
     || m_state.gp.flags.any(DxvkGraphicsPipelineFlag::HasStorageDescriptors,
                             DxvkGraphicsPipelineFlag::HasTransformFeedback)) {
      // If either the current pipeline has side effects or if there are pending
      // writes from previous draws, check for hazards. This also tracks any
      // resources written for the first time, but does not emit any barriers
      // on its own so calling this outside a render pass is safe. This also
      // implicitly dirties all state for which we need to track resource access.
      if (this->checkGraphicsHazards<Indexed, Indirect>())
        this->spillRenderPass(true);

      // The render pass flag gets reset when the render pass ends, so set it late
      m_flags.set(DxvkContextFlag::GpRenderPassSideEffects);
    }

    // Start the render pass. This must happen before any render state
    // is set up so that we can safely use secondary command buffers.
    if (!m_flags.test(DxvkContextFlag::GpRenderPassBound))
      this->startRenderPass();

    // If there are any pending clears, record them now
    if (unlikely(!m_deferredClears.empty()))
      flushClearsInline();

    if (m_flags.test(DxvkContextFlag::GpRenderPassSideEffects)) {
      // Make sure that the debug label for barrier control
      // always starts within an active render pass
      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        this->beginBarrierControlDebugRegion<VK_PIPELINE_BIND_POINT_GRAPHICS>();
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
      if (unlikely(!this->updateGraphicsPipelineState()))
        return false;
    }
    
    if (m_descriptorState.hasDirtyGraphicsSets()) {
      this->updateGraphicsShaderResources();

      if (unlikely(Resolve && m_implicitResolves.hasPendingResolves())) {
        // If implicit resolves are required for any of the shader bindings, we need
        // to discard all the state setup that we've done so far and try again
        this->spillRenderPass(true);
        this->flushImplicitResolves();

        return this->commitGraphicsState<Indexed, Indirect, false>();
      }
    }
    
    if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback))
      this->updateTransformFeedbackState();
    
    this->updateDynamicState();
    
    if (m_flags.test(DxvkContextFlag::DirtyPushConstants))
      this->updatePushConstants<VK_PIPELINE_BIND_POINT_GRAPHICS>();

    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect)
      this->trackDrawBuffer();

    return true;
  }
  
  
  template<VkPipelineBindPoint BindPoint>
  bool DxvkContext::checkResourceHazards(
    const DxvkBindingLayout&        layout,
          uint32_t                  setMask) {
    constexpr bool IsGraphics = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

    // For graphics, if we are not currently inside a render pass, we'll issue
    // a barrier anyway so checking hazards is not meaningful. Avoid some overhead
    // and only track written resources in that case.
    bool requiresBarrier = IsGraphics && !m_flags.test(DxvkContextFlag::GpRenderPassBound);

    for (auto setIndex : bit::BitMask(setMask)) {
      uint32_t bindingCount = layout.getBindingCount(setIndex);

      for (uint32_t j = 0; j < bindingCount; j++) {
        const DxvkBindingInfo& binding = layout.getBinding(setIndex, j);
        const DxvkShaderResourceSlot& slot = m_rc[binding.resourceBinding];

        // Skip read-only bindings if we already know that we need a barrier
        if (requiresBarrier && !(binding.access & vk::AccessWriteMask))
          continue;

        switch (binding.descriptorType) {
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            if (slot.bufferView) {
              if (!IsGraphics || slot.bufferView->buffer()->hasGfxStores())
                requiresBarrier |= checkBufferViewBarrier<BindPoint>(slot.bufferView, binding.access, binding.accessOp);
              else if (binding.access & vk::AccessWriteMask)
                requiresBarrier |= !slot.bufferView->buffer()->trackGfxStores();
            }
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
            if (slot.bufferView && (!IsGraphics || slot.bufferView->buffer()->hasGfxStores()))
              requiresBarrier |= checkBufferViewBarrier<BindPoint>(slot.bufferView, binding.access, DxvkAccessOp::None);
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
            if (slot.bufferSlice.length() && (!IsGraphics || slot.bufferSlice.buffer()->hasGfxStores()))
              requiresBarrier |= checkBufferBarrier<BindPoint>(slot.bufferSlice, binding.access, DxvkAccessOp::None);
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            if (slot.bufferSlice.length()) {
              if (!IsGraphics || slot.bufferSlice.buffer()->hasGfxStores())
                requiresBarrier |= checkBufferBarrier<BindPoint>(slot.bufferSlice, binding.access, binding.accessOp);
              else if (binding.access & vk::AccessWriteMask)
                requiresBarrier |= !slot.bufferSlice.buffer()->trackGfxStores();
            }
          } break;

          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            if (slot.imageView) {
              if (!IsGraphics || slot.imageView->hasGfxStores())
                requiresBarrier |= checkImageViewBarrier<BindPoint>(slot.imageView, binding.access, binding.accessOp);
              else if (binding.access & vk::AccessWriteMask)
                requiresBarrier |= !slot.imageView->image()->trackGfxStores();
            }
          } break;

          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            if (slot.imageView && (!IsGraphics || slot.imageView->hasGfxStores()))
              requiresBarrier |= checkImageViewBarrier<BindPoint>(slot.imageView, binding.access, DxvkAccessOp::None);
          } break;

          default:
            /* nothing to do */;
        }

        // We don't need to do any extra tracking for compute here, exit early
        if (requiresBarrier && !IsGraphics)
          return true;
      }
    }

    return requiresBarrier;
  }
  

  bool DxvkContext::checkComputeHazards() {
    // Exit early if we know that there cannot be any hazards to avoid
    // some overhead after barriers are flushed. This is common.
    if (m_barrierTracker.empty())
      return false;

    const auto& layout = m_state.cp.pipeline->getBindings()->layout();
    return checkResourceHazards<VK_PIPELINE_BIND_POINT_COMPUTE>(layout, layout.getSetMask());
  }


  template<bool Indexed, bool Indirect>
  bool DxvkContext::checkGraphicsHazards() {
    // Check shader resources on every draw to handle WAW hazards, and to make
    // sure that writes are handled properly. If the pipeline does not have any
    // storage descriptors, we only need to check dirty resources.
    const auto& layout = m_state.gp.pipeline->getBindings()->layout();

    uint32_t setMask = layout.getSetMask();

    if (!m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasStorageDescriptors))
      setMask &= m_descriptorState.getDirtyGraphicsSets();

    bool requiresBarrier = checkResourceHazards<VK_PIPELINE_BIND_POINT_GRAPHICS>(layout, setMask);

    // Transform feedback buffer writes won't overlap, so we also only need to
    // check those if dirty.
    if (m_flags.test(DxvkContextFlag::GpDirtyXfbBuffers)
     && m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback)) {
      for (uint32_t i = 0; i < MaxNumXfbBuffers; i++) {
        const auto& xfbBufferSlice = m_state.xfb.buffers[i];
        const auto& xfbCounterSlice = m_state.xfb.activeCounters[i];

        if (xfbBufferSlice.length()) {
          requiresBarrier |= !xfbBufferSlice.buffer()->trackGfxStores();
          requiresBarrier |= checkBufferBarrier<VK_PIPELINE_BIND_POINT_GRAPHICS>(
            xfbBufferSlice, VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, DxvkAccessOp::None);

          if (xfbCounterSlice.length()) {
            requiresBarrier |= !xfbCounterSlice.buffer()->trackGfxStores();
            requiresBarrier |= checkBufferBarrier<VK_PIPELINE_BIND_POINT_GRAPHICS>(xfbCounterSlice,
              VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
              VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
              DxvkAccessOp::None);
          }
        }
      }
    }

    // From now on, we only have read-only resources to check and can
    // exit early if we find a hazard.
    if (requiresBarrier)
      return true;

    // Check the draw buffer for indirect draw calls
    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect) {
      std::array<DxvkBufferSlice*, 2> slices = {{
        &m_state.id.argBuffer,
        &m_state.id.cntBuffer,
      }};

      for (uint32_t i = 0; i < slices.size(); i++) {
        if (slices[i]->length() && slices[i]->buffer()->hasGfxStores()) {
          if (checkBufferBarrier<VK_PIPELINE_BIND_POINT_GRAPHICS>(*slices[i],
              VK_ACCESS_INDIRECT_COMMAND_READ_BIT, DxvkAccessOp::None))
            return true;
        }
      }
    }

    // Read-only stage, so we only have to check this if
    // the bindngs have actually changed between draws
    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer) && Indexed) {
      const auto& indexBufferSlice = m_state.vi.indexBuffer;

      if (indexBufferSlice.length() && indexBufferSlice.buffer()->hasGfxStores()) {
        if (checkBufferBarrier<VK_PIPELINE_BIND_POINT_GRAPHICS>(indexBufferSlice,
            VK_ACCESS_INDEX_READ_BIT, DxvkAccessOp::None))
          return true;
      }
    }

    // Same here, also ignore unused vertex bindings
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers)) {
      uint32_t bindingCount = m_state.gp.state.il.bindingCount();

      for (uint32_t i = 0; i < bindingCount; i++) {
        uint32_t binding = m_state.gp.state.ilBindings[i].binding();
        const auto& vertexBufferSlice = m_state.vi.vertexBuffers[binding];

        if (vertexBufferSlice.length() && vertexBufferSlice.buffer()->hasGfxStores()) {
          if (checkBufferBarrier<VK_PIPELINE_BIND_POINT_GRAPHICS>(vertexBufferSlice,
              VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, DxvkAccessOp::None))
            return true;
        }
      }
    }

    return false;
  }


  template<VkPipelineBindPoint BindPoint>
  bool DxvkContext::checkBufferBarrier(
    const DxvkBufferSlice&          bufferSlice,
          VkAccessFlags             access,
          DxvkAccessOp              accessOp) {
    return checkResourceBarrier<BindPoint>([this, &bufferSlice, accessOp] (DxvkAccess access) {
      return resourceHasAccess(*bufferSlice.buffer(),
        bufferSlice.offset(), bufferSlice.length(), access, accessOp);
    }, access);
  }


  template<VkPipelineBindPoint BindPoint>
  bool DxvkContext::checkBufferViewBarrier(
    const Rc<DxvkBufferView>&       bufferView,
          VkAccessFlags             access,
          DxvkAccessOp              accessOp) {
    return checkResourceBarrier<BindPoint>([this, &bufferView, accessOp] (DxvkAccess access) {
      return resourceHasAccess(*bufferView, access, accessOp);
    }, access);
  }


  template<VkPipelineBindPoint BindPoint>
  bool DxvkContext::checkImageViewBarrier(
    const Rc<DxvkImageView>&        imageView,
          VkAccessFlags             access,
          DxvkAccessOp              accessOp) {
    return checkResourceBarrier<BindPoint>([this, &imageView, accessOp] (DxvkAccess access) {
      return resourceHasAccess(*imageView, access, accessOp);
    }, access);
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
  }


  void DxvkContext::trackDrawBuffer() {
    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer)) {
      m_flags.clr(DxvkContextFlag::DirtyDrawBuffer);

      m_renderPassBarrierSrc.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      m_renderPassBarrierSrc.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

      if (m_state.id.argBuffer.length())
        m_cmd->track(m_state.id.argBuffer.buffer(), DxvkAccess::Read);

      if (m_state.id.cntBuffer.length())
        m_cmd->track(m_state.id.cntBuffer.buffer(), DxvkAccess::Read);
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

    this->invalidateBuffer(buffer, buffer->allocateStorage());
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

    flushBarriers();

    small_vector<VkImageMemoryBarrier2, 16> imageBarriers;

    VkMemoryBarrier2 memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    memoryBarrier.dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    memoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

    for (size_t i = 0; i < bufferCount; i++) {
      const auto& info = bufferInfos[i];

      memoryBarrier.srcStageMask |= info.buffer->info().stages;
      memoryBarrier.srcAccessMask |= info.buffer->info().access;
    }

    for (size_t i = 0; i < imageCount; i++) {
      const auto& info = imageInfos[i];
      auto oldStorage = info.image->storage();

      // The source image may only be partially initialized. Ignore any subresources
      // that aren't, but try to do process as much in one go as possible.
      VkImageSubresourceRange availableSubresources = info.image->getAvailableSubresources();

      uint32_t mipStep = info.image->isInitialized(availableSubresources)
        ? availableSubresources.levelCount : 1u;

      for (uint32_t m = 0; m < availableSubresources.levelCount; m += mipStep) {
        VkImageSubresourceRange subresourceRange = availableSubresources;
        subresourceRange.baseMipLevel = m;
        subresourceRange.levelCount = mipStep;

        uint32_t layerStep = info.image->isInitialized(subresourceRange)
          ? availableSubresources.layerCount : 1u;

        for (uint32_t l = 0; l < availableSubresources.layerCount; l += layerStep) {
          subresourceRange.baseArrayLayer = l;
          subresourceRange.layerCount = layerStep;

          if (info.image->isInitialized(subresourceRange)) {
            VkImageMemoryBarrier2 dstBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            dstBarrier.srcStageMask = info.image->info().stages;
            dstBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            dstBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            dstBarrier.newLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            dstBarrier.image = info.storage->getImageInfo().image;
            dstBarrier.subresourceRange = subresourceRange;

            imageBarriers.push_back(dstBarrier);

            VkImageMemoryBarrier2 srcBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            srcBarrier.srcStageMask = info.image->info().stages;
            srcBarrier.srcAccessMask = info.image->info().access;
            srcBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            srcBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            srcBarrier.oldLayout = info.image->info().layout;
            srcBarrier.newLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            srcBarrier.image = oldStorage->getImageInfo().image;
            srcBarrier.subresourceRange = subresourceRange;

            if (srcBarrier.oldLayout != srcBarrier.newLayout) {
              imageBarriers.push_back(srcBarrier);
            } else {
              memoryBarrier.srcStageMask |= srcBarrier.srcStageMask;
              memoryBarrier.srcAccessMask |= srcBarrier.srcAccessMask;
            }
          }
        }
      }
    }

    // Submit all pending barriers in one go
    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    if (!imageBarriers.empty()) {
      depInfo.imageMemoryBarrierCount = imageBarriers.size();
      depInfo.pImageMemoryBarriers = imageBarriers.data();
    }

    if (memoryBarrier.srcStageMask) {
      depInfo.memoryBarrierCount = 1u;
      depInfo.pMemoryBarriers = &memoryBarrier;
    }

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    // Set up post-copy barriers
    depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    memoryBarrier.srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    memoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;

    imageBarriers.clear();

    // Copy and invalidate all buffers
    for (size_t i = 0; i < bufferCount; i++) {
      const auto& info = bufferInfos[i];
      auto oldStorage = info.buffer->storage();

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

      invalidateBuffer(info.buffer, Rc<DxvkResourceAllocation>(info.storage));

      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &copy);
      m_cmd->track(info.buffer, DxvkAccess::Write);

      memoryBarrier.dstStageMask |= info.buffer->info().stages;
      memoryBarrier.dstAccessMask |= info.buffer->info().access;
    }

    // Copy and invalidate all images
    for (size_t i = 0; i < imageCount; i++) {
      const auto& info = imageInfos[i];
      auto oldStorage = info.image->storage();

      DxvkResourceImageInfo dstInfo = info.storage->getImageInfo();
      DxvkResourceImageInfo srcInfo = oldStorage->getImageInfo();

      VkImageSubresourceRange availableSubresources = info.image->getAvailableSubresources();

      // Iterate over all subresources and compute copy regions. We need
      // one region per mip or plane, so size the local array accordingly.
      small_vector<VkImageCopy2, 16> imageRegions;

      uint32_t planeCount = 1;

      auto formatInfo = info.image->formatInfo();

      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane))
        planeCount = vk::getPlaneCount(formatInfo->aspectMask);

      for (uint32_t p = 0; p < planeCount; p++) {
        for (uint32_t m = 0; m < info.image->info().mipLevels; m++) {
          VkImageSubresourceRange subresourceRange = availableSubresources;
          subresourceRange.baseMipLevel = m;
          subresourceRange.levelCount = 1u;

          uint32_t layerStep = info.image->isInitialized(subresourceRange)
            ? subresourceRange.layerCount : 1u;

          for (uint32_t l = 0; l < subresourceRange.layerCount; l += layerStep) {
            subresourceRange.baseArrayLayer = l;
            subresourceRange.layerCount = layerStep;

            if (info.image->isInitialized(subresourceRange)) {
              VkImageCopy2 region = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
              region.dstSubresource.aspectMask = formatInfo->aspectMask;
              region.dstSubresource.mipLevel = m;
              region.dstSubresource.baseArrayLayer = l;
              region.dstSubresource.layerCount = layerStep;
              region.srcSubresource = region.dstSubresource;
              region.extent = info.image->mipLevelExtent(m);

              if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
                region.dstSubresource.aspectMask = vk::getPlaneAspect(p);
                region.srcSubresource.aspectMask = vk::getPlaneAspect(p);

                region.extent.width /= formatInfo->planes[p].blockSize.width;
                region.extent.height /= formatInfo->planes[p].blockSize.height;
              }

              imageRegions.push_back(region);

              // Emit image barrier for this region. We could in theory transition the
              // entire image in one go as long as all subresources are initialized,
              // but there is usually no reason to do so.
              VkImageMemoryBarrier2 dstBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
              dstBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
              dstBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
              dstBarrier.dstStageMask = info.image->info().stages;
              dstBarrier.dstAccessMask = info.image->info().access;
              dstBarrier.oldLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
              dstBarrier.newLayout = info.image->info().layout;
              dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
              dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
              dstBarrier.image = info.storage->getImageInfo().image;
              dstBarrier.subresourceRange = vk::makeSubresourceRange(region.dstSubresource);

              if (dstBarrier.oldLayout != dstBarrier.newLayout) {
                imageBarriers.push_back(dstBarrier);
              } else {
                memoryBarrier.dstStageMask |= dstBarrier.dstStageMask;
                memoryBarrier.dstAccessMask |= dstBarrier.dstAccessMask;
              }
            }
          }
        }
      }

      VkCopyImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
      copy.dstImage = dstInfo.image;
      copy.dstImageLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      copy.srcImage = srcInfo.image;
      copy.srcImageLayout = info.image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      copy.regionCount = imageRegions.size();
      copy.pRegions = imageRegions.data();

      invalidateImageWithUsage(info.image, Rc<DxvkResourceAllocation>(info.storage), info.usageInfo);

      m_cmd->cmdCopyImage(DxvkCmdBuffer::ExecBuffer, &copy);
      m_cmd->track(info.image, DxvkAccess::Write);
    }

    if (!imageBarriers.empty()) {
      depInfo.imageMemoryBarrierCount = imageBarriers.size();
      depInfo.pImageMemoryBarriers = imageBarriers.data();
    }

    if (memoryBarrier.dstStageMask) {
      depInfo.memoryBarrierCount = 1u;
      depInfo.pMemoryBarriers = &memoryBarrier;
    }

    m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
  }


  void DxvkContext::relocateQueuedResources() {
    // Limit the number and size of resources to process per submission to
    // something reasonable. We don't know if we are transferring over PCIe.
    constexpr static uint32_t MaxRelocationsPerSubmission = 128u;
    constexpr static uint32_t MaxRelocatedMemoryPerSubmission = 16u << 20;

    auto resourceList = m_common->memoryManager().pollRelocationList(
      MaxRelocationsPerSubmission, MaxRelocatedMemoryPerSubmission);

    if (resourceList.empty())
      return;

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xc0a2f0, "Memory defrag"));
    }

    std::vector<DxvkRelocateBufferInfo> bufferInfos;
    std::vector<DxvkRelocateImageInfo> imageInfos;

    // Iterate over resource list and try to create and assign new allocations
    // for them based on the mode selected by the allocator. Failures here are
    // not fatal, but may lead to weird behaviour down the line - ignore for now.
    for (const auto& e : resourceList) {
      auto storage = e.resource->relocateStorage(e.mode);

      if (!storage)
        continue;

      Rc<DxvkImage> image = dynamic_cast<DxvkImage*>(e.resource.ptr());
      Rc<DxvkBuffer> buffer = dynamic_cast<DxvkBuffer*>(e.resource.ptr());

      if (image) {
        auto& e = imageInfos.emplace_back();
        e.image = std::move(image);
        e.storage = std::move(storage);
      } else if (buffer) {
        auto& e = bufferInfos.emplace_back();
        e.buffer = std::move(buffer);
        e.storage = std::move(storage);
      }
    }

    if (bufferInfos.empty() && imageInfos.empty())
      return;

    // If there are any resources to relocate, we have to stall the transfer
    // queue so that subsequent resource uploads do not overlap with resource
    // copies on the graphics timeline.
    relocateResources(
      bufferInfos.size(), bufferInfos.data(),
      imageInfos.size(), imageInfos.data());

    m_cmd->setSubmissionBarrier();

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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
    if (m_zeroBuffer && m_zeroBuffer->info().size >= size) {
      m_cmd->track(m_zeroBuffer, DxvkAccess::Read);
      return m_zeroBuffer;
    }

    DxvkBufferCreateInfo bufInfo;
    bufInfo.size    = align<VkDeviceSize>(size, 1 << 20);
    bufInfo.usage   = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.stages  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufInfo.access  = VK_ACCESS_TRANSFER_WRITE_BIT
                    | VK_ACCESS_TRANSFER_READ_BIT;
    bufInfo.debugName = "Zero buffer";

    m_zeroBuffer = m_device->createBuffer(bufInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkBufferSliceHandle slice = m_zeroBuffer->getSliceHandle();

    // FillBuffer is allowed even on transfer queues. Execute it on the barrier
    // command buffer to ensure that subsequent transfer commands can see it.
    m_cmd->cmdFillBuffer(DxvkCmdBuffer::SdmaBarriers,
      slice.handle, slice.offset, slice.length, 0);

    accessMemory(DxvkCmdBuffer::SdmaBarriers,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

    if (m_device->hasDedicatedTransferQueue()) {
      accessMemory(DxvkCmdBuffer::InitBarriers,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    }

    m_cmd->track(m_zeroBuffer, DxvkAccess::Write);
    return m_zeroBuffer;
  }


  void DxvkContext::freeZeroBuffer() {
    constexpr uint64_t ZeroBufferLifetime = 4096u;

    // Don't free the zero buffer if it is still kept alive by a prior
    // submission anyway
    if (!m_zeroBuffer || m_zeroBuffer->isInUse(DxvkAccess::Write))
      return;

    // Delete zero buffer if it hasn't been actively used in a while
    if (m_zeroBuffer->getTrackId() + ZeroBufferLifetime < m_trackingId)
      m_zeroBuffer = nullptr;
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


  void DxvkContext::flushImplicitResolves() {
    spillRenderPass(true);

    DxvkImplicitResolveOp op;

    while (m_implicitResolves.extractResolve(op)) {
      prepareImage(op.inputImage, vk::makeSubresourceRange(op.resolveRegion.srcSubresource));
      prepareImage(op.resolveImage, vk::makeSubresourceRange(op.resolveRegion.dstSubresource));

      // Always do a SAMPLE_ZERO resolve here since that's less expensive and closer to what
      // happens on native AMD anyway. Need to use a shader in case we are dealing with a
      // non-integer color image since render pass resolves only support AVERAGE..
      // We know that the source image is shader readable and that the resolve image can be
      // rendered to, so reduce the image compatibility checks to a minimum here.
      bool useRp = (getDefaultResolveMode(op.resolveFormat) == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) &&
        (op.inputImage->info().usage & (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

      if (useRp) {
        resolveImageRp(op.resolveImage, op.inputImage, op.resolveRegion,
          op.resolveFormat, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
      } else {
        resolveImageFb(op.resolveImage, op.inputImage, op.resolveRegion,
          op.resolveFormat, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
      }
    }
  }


  void DxvkContext::beginCurrentCommands() {
    beginActiveDebugRegions();

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

    m_cmd->setTrackingId(++m_trackingId);
  }


  void DxvkContext::endCurrentCommands() {
    this->spillRenderPass(true);
    this->flushSharedImages();

    m_sdmaAcquires.finalize(m_cmd);
    m_sdmaBarriers.finalize(m_cmd);
    m_initAcquires.finalize(m_cmd);
    m_initBarriers.finalize(m_cmd);
    m_execBarriers.finalize(m_cmd);

    m_barrierTracker.clear();

    endActiveDebugRegions();
  }


  void DxvkContext::splitCommands() {
    // This behaves the same as a pair of endRecording and
    // beginRecording calls, except that we keep the same
    // command list object for subsequent commands.
    this->endCurrentCommands();

    m_cmd->next();

    this->beginCurrentCommands();
  }


  void DxvkContext::discardRenderTarget(
    const DxvkImage&                image,
    const VkImageSubresourceRange&  subresources) {
    // We can only retroactively change store ops if we are currently
    // inside a render pass that uses secondary command buffers.
    if (!m_flags.test(DxvkContextFlag::GpRenderPassSecondaryCmd))
      return;

    // Handling 3D is possible, but annoying, so skip it
    if (image.info().type == VK_IMAGE_TYPE_3D)
      return;

    for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
      auto& view = m_state.om.framebufferInfo.getAttachment(i).view;

      if (view->image() != &image)
        continue;

      // If the given subresource range fully contains any bound render target,
      // retroactively change the corresponding store op to DONT_CARE.
      auto viewSubresources = view->imageSubresources();

      if (vk::checkSubresourceRangeSuperset(subresources, viewSubresources))
        deferDiscard(view, subresources.aspectMask);
    }
  }


  void DxvkContext::flushImageLayoutTransitions(
          DxvkCmdBuffer             cmdBuffer) {
    if (m_imageLayoutTransitions.empty())
      return;

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
      depInfo.imageMemoryBarrierCount = m_imageLayoutTransitions.size();
      depInfo.pImageMemoryBarriers = m_imageLayoutTransitions.data();

      m_cmd->cmdPipelineBarrier(cmdBuffer, &depInfo);
    } else {
      // If we're recording into an out-of-order command buffer, batch
      // layout transitions into a dedicated command buffer in order to
      // avoid pipeline stalls.
      DxvkCmdBuffer barrierBuffer = cmdBuffer;

      if (cmdBuffer == DxvkCmdBuffer::InitBuffer)
        barrierBuffer = DxvkCmdBuffer::InitBarriers;
      if (cmdBuffer == DxvkCmdBuffer::SdmaBuffer)
        barrierBuffer = DxvkCmdBuffer::SdmaBarriers;

      auto& batch = getBarrierBatch(barrierBuffer);

      for (const auto& barrier : m_imageLayoutTransitions)
        batch.addImageBarrier(barrier);
    }

    m_imageLayoutTransitions.clear();
  }


  void DxvkContext::addImageLayoutTransition(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess) {
    if (srcLayout == dstLayout && srcStages == dstStages)
      return;

    if (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED || srcLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
      image.trackInitialization(subresources);

    auto& barrier = m_imageLayoutTransitions.emplace_back();
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.handle();
    barrier.subresourceRange = subresources;
  }


  void DxvkContext::addImageLayoutTransition(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          bool                      discard) {
    // If discard is false, this assumes that the image is in its default
    // layout and ready to be accessed via its standard access patterns.
    VkImageLayout srcLayout = image.info().layout;

    if (discard) {
      // Only discard if the image is either uninitialized or if it is
      // GPU-writable. Discarding is most likely not useful otherwise.
      constexpr VkImageUsageFlags WritableFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;

      if ((image.info().usage & WritableFlags) || !image.isInitialized(subresources))
        srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    addImageLayoutTransition(image, subresources,
      srcLayout, dstStages, 0,
      dstLayout, dstStages, dstAccess);
  }


  void DxvkContext::addImageInitTransition(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess) {
    addImageLayoutTransition(image, subresources,
      VK_IMAGE_LAYOUT_UNDEFINED, 0, 0,
      dstLayout, dstStages, dstAccess);
  }


  void DxvkContext::accessMemory(
          DxvkCmdBuffer             cmdBuffer,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess) {
    auto& batch = getBarrierBatch(cmdBuffer);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;

    batch.addMemoryBarrier(barrier);
  }


  void DxvkContext::accessImage(
          DxvkCmdBuffer             cmdBuffer,
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessImage(cmdBuffer, image, subresources,
      srcLayout, srcStages, srcAccess,
      image.info().layout,
      image.info().stages,
      image.info().access,
      accessOp);
  }


  void DxvkContext::accessImage(
          DxvkCmdBuffer             cmdBuffer,
    const DxvkImageView&            imageView,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessImage(cmdBuffer, *imageView.image(),
      imageView.imageSubresources(),
      imageView.image()->info().layout,
      srcStages, srcAccess, accessOp);
  }


  void DxvkContext::accessImage(
          DxvkCmdBuffer             cmdBuffer,
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          DxvkAccessOp              accessOp) {
    auto& batch = getBarrierBatch(cmdBuffer);

    if (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED || srcLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
      image.trackInitialization(subresources);

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.handle();
    barrier.subresourceRange = subresources;

    batch.addImageBarrier(barrier);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      bool hasWrite = (srcAccess & vk::AccessWriteMask) || (srcLayout != dstLayout);
      bool hasRead = (srcAccess & vk::AccessReadMask);

      uint32_t layerCount = image.info().numLayers;

      DxvkAddressRange range;
      range.resource = image.getResourceId();
      range.accessOp = accessOp;

      if (subresources.levelCount == 1u || subresources.layerCount == layerCount) {
        range.rangeStart = image.getTrackingAddress(
          subresources.baseMipLevel, subresources.baseArrayLayer);
        range.rangeEnd = image.getTrackingAddress(
          subresources.baseMipLevel + subresources.levelCount,
          subresources.baseArrayLayer + subresources.layerCount) - 1u;

        if (hasWrite)
          m_barrierTracker.insertRange(range, DxvkAccess::Write);
        if (hasRead)
          m_barrierTracker.insertRange(range, DxvkAccess::Read);
      } else {
        for (uint32_t i = subresources.baseMipLevel; i < subresources.baseMipLevel + subresources.levelCount; i++) {
          range.rangeStart = image.getTrackingAddress(i, subresources.baseArrayLayer);
          range.rangeEnd = image.getTrackingAddress(i, subresources.baseArrayLayer + subresources.layerCount) - 1u;

          if (hasWrite)
            m_barrierTracker.insertRange(range, DxvkAccess::Write);
          if (hasRead)
            m_barrierTracker.insertRange(range, DxvkAccess::Read);
        }
      }
    }
  }


  void DxvkContext::accessImageRegion(
          DxvkCmdBuffer             cmdBuffer,
          DxvkImage&                image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                offset,
          VkExtent3D                extent,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessImageRegion(cmdBuffer, image, subresources,
      offset, extent, srcLayout, srcStages, srcAccess,
      image.info().layout, image.info().stages, image.info().access,
      accessOp);
  }


  void DxvkContext::accessImageRegion(
          DxvkCmdBuffer             cmdBuffer,
          DxvkImage&                image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                offset,
          VkExtent3D                extent,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          DxvkAccessOp              accessOp) {
    // If the image layout needs to change, access the entire image
    if (srcLayout != dstLayout) {
      accessImage(cmdBuffer, image, vk::makeSubresourceRange(subresources),
        srcLayout, srcStages, srcAccess,
        dstLayout, dstStages, dstAccess, accessOp);
      return;
    }

    // No layout transition, just emit a plain memory barrier
    auto& batch = getBarrierBatch(cmdBuffer);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;

    batch.addMemoryBarrier(barrier);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      bool hasWrite = (srcAccess & vk::AccessWriteMask) || (srcLayout != dstLayout);
      bool hasRead = (srcAccess & vk::AccessReadMask);

      DxvkAddressRange range;
      range.resource = image.getResourceId();
      range.accessOp = accessOp;

      if (extent == image.mipLevelExtent(subresources.mipLevel)) {
        range.rangeStart = image.getTrackingAddress(subresources.mipLevel, subresources.baseArrayLayer);
        range.rangeEnd = image.getTrackingAddress(subresources.mipLevel, subresources.baseArrayLayer + subresources.layerCount) - 1u;

        if (hasWrite)
          m_barrierTracker.insertRange(range, DxvkAccess::Write);
        if (hasRead)
          m_barrierTracker.insertRange(range, DxvkAccess::Read);
      } else {
        VkOffset3D maxCoord = offset;
        maxCoord.x += extent.width - 1u;
        maxCoord.y += extent.height - 1u;
        maxCoord.z += extent.depth - 1u;

        for (uint32_t i = subresources.baseArrayLayer; i < subresources.baseArrayLayer + subresources.layerCount; i++) {
          range.rangeStart = image.getTrackingAddress(subresources.mipLevel, i, offset);
          range.rangeEnd = image.getTrackingAddress(subresources.mipLevel, i, maxCoord);

          if (hasWrite)
            m_barrierTracker.insertRange(range, DxvkAccess::Write);
          if (hasRead)
            m_barrierTracker.insertRange(range, DxvkAccess::Read);
        }
      }
    }
  }


  void DxvkContext::accessImageTransfer(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess) {
    auto& transferBatch = getBarrierBatch(DxvkCmdBuffer::SdmaBuffer);
    auto& graphicsBatch = getBarrierBatch(DxvkCmdBuffer::InitBarriers);

    if (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED || srcLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
      image.trackInitialization(subresources);

    if (m_device->hasDedicatedTransferQueue()) {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask = srcStages;
      barrier.srcAccessMask = srcAccess;
      barrier.dstStageMask = srcStages;
      barrier.dstAccessMask = VK_ACCESS_2_NONE;
      barrier.oldLayout = srcLayout;
      barrier.newLayout = image.info().layout;
      barrier.srcQueueFamilyIndex = m_device->queues().transfer.queueFamily;
      barrier.dstQueueFamilyIndex = m_device->queues().graphics.queueFamily;
      barrier.image = image.handle();
      barrier.subresourceRange = subresources;

      transferBatch.addImageBarrier(barrier);

      barrier.srcAccessMask = VK_ACCESS_2_NONE;
      barrier.dstStageMask = image.info().stages;
      barrier.dstAccessMask = image.info().access;

      graphicsBatch.addImageBarrier(barrier);
    } else {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask = srcStages;
      barrier.srcAccessMask = srcAccess;
      barrier.dstStageMask = image.info().stages;
      barrier.dstAccessMask = image.info().access;
      barrier.oldLayout = srcLayout;
      barrier.newLayout = image.info().layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = image.handle();
      barrier.subresourceRange = subresources;

      transferBatch.addImageBarrier(barrier);
    }
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
          DxvkBuffer&               buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessBuffer(cmdBuffer, buffer, offset, size,
      srcStages, srcAccess,
      buffer.info().stages,
      buffer.info().access,
      accessOp);
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
          DxvkBuffer&               buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          DxvkAccessOp              accessOp) {
    if (unlikely(!size))
      return;

    auto& batch = getBarrierBatch(cmdBuffer);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStages;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStages;
    barrier.dstAccessMask = dstAccess;

    batch.addMemoryBarrier(barrier);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      DxvkAddressRange range;
      range.resource = buffer.getResourceId();
      range.accessOp = accessOp;
      range.rangeStart = offset;
      range.rangeEnd = offset + size - 1;

      if (srcAccess & vk::AccessWriteMask)
        m_barrierTracker.insertRange(range, DxvkAccess::Write);
      if (srcAccess & vk::AccessReadMask)
        m_barrierTracker.insertRange(range, DxvkAccess::Read);
    }
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
    const DxvkBufferSlice&          bufferSlice,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessBuffer(cmdBuffer,
      *bufferSlice.buffer(),
      bufferSlice.offset(),
      bufferSlice.length(),
      srcStages, srcAccess,
      accessOp);
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
    const DxvkBufferSlice&          bufferSlice,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          DxvkAccessOp              accessOp) {
    accessBuffer(cmdBuffer,
      *bufferSlice.buffer(),
      bufferSlice.offset(),
      bufferSlice.length(),
      srcStages, srcAccess,
      dstStages, dstAccess,
      accessOp);
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
          DxvkBufferView&           bufferView,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          DxvkAccessOp              accessOp) {
    accessBuffer(cmdBuffer,
      *bufferView.buffer(),
      bufferView.info().offset,
      bufferView.info().size,
      srcStages, srcAccess, accessOp);
  }


  void DxvkContext::accessBuffer(
          DxvkCmdBuffer             cmdBuffer,
          DxvkBufferView&           bufferView,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          DxvkAccessOp              accessOp) {
    accessBuffer(cmdBuffer,
      *bufferView.buffer(),
      bufferView.info().offset,
      bufferView.info().size,
      srcStages, srcAccess,
      dstStages, dstAccess,
      accessOp);
  }


  void DxvkContext::accessBufferTransfer(
          DxvkBuffer&               buffer,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess) {
    auto& transferBatch = getBarrierBatch(DxvkCmdBuffer::SdmaBuffer);
    auto& graphicsBatch = getBarrierBatch(DxvkCmdBuffer::InitBarriers);

    if (m_device->hasDedicatedTransferQueue()) {
      // No queue ownership transfer necessary since buffers all
      // use SHARING_MODE_CONCURRENT, but we need a split barrier.
      VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
      barrier.srcStageMask = srcStages;
      barrier.srcAccessMask = srcAccess;
      barrier.dstStageMask = srcStages;
      barrier.dstAccessMask = VK_ACCESS_2_NONE;

      transferBatch.addMemoryBarrier(barrier);

      barrier.srcStageMask = srcStages;
      barrier.srcAccessMask = VK_ACCESS_2_NONE;
      barrier.dstStageMask = buffer.info().stages;
      barrier.dstAccessMask = buffer.info().access;

      graphicsBatch.addMemoryBarrier(barrier);
    } else {
      VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
      barrier.srcStageMask = srcStages;
      barrier.srcAccessMask = srcAccess;
      barrier.dstStageMask = buffer.info().stages;
      barrier.dstAccessMask = buffer.info().access;

      transferBatch.addMemoryBarrier(barrier);
    }
  }


  void DxvkContext::accessDrawBuffer(
          VkDeviceSize              offset,
          uint32_t                  count,
          uint32_t                  stride,
          uint32_t                  size) {
    uint32_t dataSize = count ? (count - 1u) * stride + size : 0u;

    accessBuffer(DxvkCmdBuffer::ExecBuffer,
      *m_state.id.argBuffer.buffer(),
      m_state.id.argBuffer.offset() + offset, dataSize,
      VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,
      DxvkAccessOp::None);
  }


  void DxvkContext::accessDrawCountBuffer(
          VkDeviceSize              offset) {
    accessBuffer(DxvkCmdBuffer::ExecBuffer,
      *m_state.id.cntBuffer.buffer(),
      m_state.id.cntBuffer.offset() + offset, sizeof(uint32_t),
      VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,
      DxvkAccessOp::None);
  }


  void DxvkContext::flushPendingAccesses(
          DxvkBuffer&               buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          DxvkAccess                access) {
    bool flush = resourceHasAccess(buffer, offset, size,
      DxvkAccess::Write, DxvkAccessOp::None);

    if (access == DxvkAccess::Write && !flush) {
      flush = resourceHasAccess(buffer, offset, size,
        DxvkAccess::Read, DxvkAccessOp::None);
    }

    if (flush)
      flushBarriers();
  }


  void DxvkContext::flushPendingAccesses(
          DxvkBufferView&           bufferView,
          DxvkAccess                access) {
    flushPendingAccesses(*bufferView.buffer(),
      bufferView.info().offset,
      bufferView.info().size,
      access);
  }


  void DxvkContext::flushPendingAccesses(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          DxvkAccess                access) {
    bool flush = resourceHasAccess(image, subresources,
      DxvkAccess::Write, DxvkAccessOp::None);

    if (access == DxvkAccess::Write && !flush) {
      flush = resourceHasAccess(image, subresources,
        DxvkAccess::Read, DxvkAccessOp::None);
    }

    if (flush)
      flushBarriers();
  }


  void DxvkContext::flushPendingAccesses(
          DxvkImage&                image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                offset,
          VkExtent3D                extent,
          DxvkAccess                access) {
    bool flush = resourceHasAccess(image, subresources,
      offset, extent, DxvkAccess::Write, DxvkAccessOp::None);

    if (access == DxvkAccess::Write && !flush) {
      flush = resourceHasAccess(image, subresources,
        offset, extent, DxvkAccess::Read, DxvkAccessOp::None);
    }

    if (flush)
      flushBarriers();
  }


  void DxvkContext::flushPendingAccesses(
          DxvkImageView&            imageView,
          DxvkAccess                access) {
    flushPendingAccesses(*imageView.image(),
      imageView.imageSubresources(), access);
  }


  void DxvkContext::flushBarriers() {
    m_execBarriers.flush(m_cmd);
    m_barrierTracker.clear();

    m_flags.clr(DxvkContextFlag::ForceWriteAfterWriteSync);
  }


  bool DxvkContext::resourceHasAccess(
          DxvkBuffer&               buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          DxvkAccess                access,
          DxvkAccessOp              accessOp) {
    if (unlikely(!size))
      return false;

    DxvkAddressRange range;
    range.resource = buffer.getResourceId();
    range.accessOp = accessOp;
    range.rangeStart = offset;
    range.rangeEnd = offset + size - 1;

    return m_barrierTracker.findRange(range, access);
  }


  bool DxvkContext::resourceHasAccess(
          DxvkBufferView&           bufferView,
          DxvkAccess                access,
          DxvkAccessOp              accessOp) {
    return resourceHasAccess(*bufferView.buffer(),
      bufferView.info().offset,
      bufferView.info().size, access, accessOp);
  }


  bool DxvkContext::resourceHasAccess(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          DxvkAccess                access,
          DxvkAccessOp              accessOp) {
    uint32_t layerCount = image.info().numLayers;

    // Subresources are enumerated in such a way that array layers of
    // one mip form a consecutive address range, and we do not track
    // individual image aspects. This is useful since image views for
    // rendering and compute can only access one mip level.
    DxvkAddressRange range;
    range.resource = image.getResourceId();
    range.accessOp = accessOp;
    range.rangeStart = image.getTrackingAddress(
      subresources.baseMipLevel, subresources.baseArrayLayer);
    range.rangeEnd = image.getTrackingAddress(
      subresources.baseMipLevel + subresources.levelCount,
      subresources.baseArrayLayer + subresources.layerCount) - 1u;

    // Probe all subresources first, only check individual mip levels
    // if there are overlaps and if we are checking a subset of array
    // layers of multiple mips.
    bool dirty = m_barrierTracker.findRange(range, access);

    if (!dirty || subresources.levelCount == 1u || subresources.layerCount == layerCount)
      return dirty;

    for (uint32_t i = subresources.baseMipLevel; i < subresources.baseMipLevel + subresources.levelCount && !dirty; i++) {
      range.rangeStart = image.getTrackingAddress(i, subresources.baseArrayLayer);
      range.rangeEnd = image.getTrackingAddress(i, subresources.baseArrayLayer + subresources.layerCount) - 1u;

      dirty = m_barrierTracker.findRange(range, access);
    }

    return dirty;
  }


  bool DxvkContext::resourceHasAccess(
          DxvkImage&                image,
    const VkImageSubresourceLayers& subresources,
          VkOffset3D                offset,
          VkExtent3D                extent,
          DxvkAccess                access,
          DxvkAccessOp              accessOp) {
    DxvkAddressRange range;
    range.resource = image.getResourceId();
    range.accessOp = accessOp;

    // If there are multiple subresources, check whether any of them have been
    // touched before checking individual regions. If the given region covers
    // the entire image, there is also no need to check more granularly.
    bool isFullSize = image.mipLevelExtent(subresources.mipLevel) == extent;

    if (subresources.layerCount > 1u || isFullSize) {
      range.rangeStart = image.getTrackingAddress(subresources.mipLevel, subresources.baseArrayLayer);
      range.rangeEnd = image.getTrackingAddress(subresources.mipLevel, subresources.baseArrayLayer + subresources.layerCount) - 1u;

      bool dirty = m_barrierTracker.findRange(range, access);

      if (!dirty || isFullSize)
        return dirty;
    }

    // Check given image region in each subresource
    VkOffset3D maxCoord = offset;
    maxCoord.x += extent.width - 1u;
    maxCoord.y += extent.height - 1u;
    maxCoord.z += extent.depth - 1u;

    for (uint32_t i = subresources.baseArrayLayer; i < subresources.baseArrayLayer + subresources.layerCount; i++) {
      range.rangeStart = image.getTrackingAddress(subresources.mipLevel, i, offset);
      range.rangeEnd = image.getTrackingAddress(subresources.mipLevel, i, maxCoord);

      if (m_barrierTracker.findRange(range, access))
        return true;
    }

    return false;
  }


  bool DxvkContext::resourceHasAccess(
          DxvkImageView&            imageView,
          DxvkAccess                access,
          DxvkAccessOp              accessOp) {
    return resourceHasAccess(*imageView.image(), imageView.imageSubresources(), access, accessOp);
  }


  DxvkBarrierBatch& DxvkContext::getBarrierBatch(
          DxvkCmdBuffer             cmdBuffer) {
    switch (cmdBuffer) {
      default:
      case DxvkCmdBuffer::ExecBuffer: return m_execBarriers;
      case DxvkCmdBuffer::InitBuffer: return m_initBarriers;
      case DxvkCmdBuffer::InitBarriers: return m_initAcquires;
      case DxvkCmdBuffer::SdmaBuffer: return m_sdmaBarriers;
      case DxvkCmdBuffer::SdmaBarriers: return m_sdmaAcquires;
    }
  }


  bool DxvkContext::prepareOutOfOrderTransfer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          DxvkAccess                access) {
    // If the resource hasn't been used yet or both uses are reads,
    // we can use this buffer in the init command buffer
    if (!buffer->isTracked(m_trackingId, access))
      return true;

    // Otherwise, our only option is to discard. We can only do that if
    // we're writing the full buffer. Therefore, the resource being read
    // should always be checked first to avoid unnecessary discards.
    if (access != DxvkAccess::Write || size < buffer->info().size || offset)
      return false;

    // Check if the buffer can actually be discarded at all.
    if (!buffer->canRelocate())
      return false;

    // Ignore large buffers to keep memory overhead in check. Use a higher
    // threshold when a render pass is active to avoid interrupting it.
    VkDeviceSize threshold = !m_flags.test(DxvkContextFlag::GpRenderPassBound)
      ? MaxDiscardSizeInRp
      : MaxDiscardSize;

    if (size > threshold)
      return false;

    // If the buffer is used for transform feedback in any way, we have to stop
    // the render pass anyway, but we can at least avoid an extra barrier.
    if (unlikely(m_flags.test(DxvkContextFlag::GpXfbActive))) {
      VkBufferUsageFlags xfbUsage = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT
                                  | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;

      if (buffer->info().usage & xfbUsage)
        this->spillRenderPass(true);
    }

    // Actually allocate and assign new backing storage
    this->invalidateBuffer(buffer, buffer->allocateStorage());
    return true;
  }


  bool DxvkContext::prepareOutOfOrderTransfer(
    const Rc<DxvkBufferView>&       bufferView,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          DxvkAccess                access) {
    if (bufferView->info().format) {
      offset *= bufferView->formatInfo()->elementSize;
      size *= bufferView->formatInfo()->elementSize;
    }

    return prepareOutOfOrderTransfer(bufferView->buffer(),
      offset + bufferView->info().offset, size, access);
  }


  bool DxvkContext::prepareOutOfOrderTransfer(
    const Rc<DxvkImage>&            image,
          DxvkAccess                access) {
    // If the image can be used for rendering, some or all subresources
    // might be in a non-default layout, and copies to or from render
    // targets typically depend on rendering operations anyway. Skip.
    if (image->info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      return false;

    // If the image hasn't been used yet or all uses are
    // reads, we can use it in the init command buffer
    return !image->isTracked(m_trackingId, access);
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


  bool DxvkContext::formatsAreResolveCompatible(
          VkFormat                  resolveFormat,
          VkFormat                  viewFormat) {
    if (resolveFormat == viewFormat)
      return true;

    static const std::array<std::pair<VkFormat, VkFormat>, 3> s_pairs = {{
      { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
      { VK_FORMAT_A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_SRGB_PACK32 },
    }};

    for (const auto& p : s_pairs) {
      if ((p.first == resolveFormat && p.second == viewFormat)
       || (p.first == viewFormat && p.second == resolveFormat))
        return true;
    }

    return false;
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


  void DxvkContext::pushDebugRegion(const VkDebugUtilsLabelEXT& label, util::DxvkDebugLabelType type) {
    m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, label);
    m_debugLabelStack.emplace_back(label, type);
  }


  void DxvkContext::popDebugRegion(util::DxvkDebugLabelType type) {
    // Find last active region of the given type
    size_t index = m_debugLabelStack.size();

    while (index && m_debugLabelStack[index - 1u].type() != type)
      index -= 1u;

    if (!index)
      return;

    // End all debug regions inside the scope we want to end, as
    // well as the debug region of the requested type itself
    for (size_t i = index; i <= m_debugLabelStack.size(); i++)
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    // Re-emit nested debug regions and erase the region we ended
    for (size_t i = index; i < m_debugLabelStack.size(); i++) {
      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, m_debugLabelStack[i].get());
      m_debugLabelStack[i - 1u] = m_debugLabelStack[i];
    }

    m_debugLabelStack.pop_back();
  }


  bool DxvkContext::hasDebugRegion(
          util::DxvkDebugLabelType    type) {
    auto e = std::find_if(m_debugLabelStack.crbegin(), m_debugLabelStack.crend(),
      [type] (const util::DxvkDebugLabel& label) { return label.type() == type; });
    return e != m_debugLabelStack.crend();
  }


  void DxvkContext::beginActiveDebugRegions() {
    for (const auto& region : m_debugLabelStack)
      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, region.get());
  }


  void DxvkContext::endActiveDebugRegions() {
    for (size_t i = 0; i < m_debugLabelStack.size(); i++)
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
  }

}
