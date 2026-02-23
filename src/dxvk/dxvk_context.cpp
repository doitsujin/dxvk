#include <algorithm>
#include <cmath>
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
    m_sdmaAcquires(*device, DxvkCmdBuffer::SdmaBarriers),
    m_sdmaBarriers(*device, DxvkCmdBuffer::SdmaBuffer),
    m_initAcquires(*device, DxvkCmdBuffer::InitBarriers),
    m_initBarriers(*device, DxvkCmdBuffer::InitBuffer),
    m_execBarriers(*device, DxvkCmdBuffer::ExecBuffer),
    m_queryManager(m_common->queryPool()),
    m_descriptorWorker(device),
    m_implicitResolves(device) {
    // Create descriptor heap or legacy pool object,
    // depending on feature support.
    if (device->canUseDescriptorHeap()) {
      m_descriptorHeap = new DxvkResourceDescriptorHeap(device.ptr());

      m_features.set(DxvkContextFeature::DescriptorHeap);
    } else if (device->canUseDescriptorBuffer()) {
      m_descriptorHeap = new DxvkResourceDescriptorHeap(device.ptr());

      m_features.set(DxvkContextFeature::DescriptorBuffer);
    } else {
      m_descriptorPool = new DxvkDescriptorPool(device.ptr());
    }

    // Init framebuffer info with default render pass in case
    // the app does not explicitly bind any render targets
    m_state.om.framebufferInfo = makeFramebufferInfo(m_state.om.renderTargets);

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

    this->beginCurrentCommands();
  }
  
  
  Rc<DxvkCommandList> DxvkContext::endRecording(
    const VkDebugUtilsLabelEXT*       reason) {
    this->endCurrentCommands();
    this->relocateQueuedResources();

    m_implicitResolves.cleanup(m_trackingId);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      // Make sure to emit the submission reason always at the very end
      if (reason && reason->pLabelName && reason->pLabelName[0])
        m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, *reason);
    }

    m_cmd->finalize();
    return std::exchange(m_cmd, nullptr);
  }


  void DxvkContext::endFrame() {
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
    // Flush pending descriptor updates and assign the sync
    // point to the submission
    if (m_features.any(DxvkContextFeature::DescriptorHeap,
                       DxvkContextFeature::DescriptorBuffer))
      m_cmd->setDescriptorSyncHandle(m_descriptorWorker.getSyncHandle());

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


  Rc<DxvkCommandList> DxvkContext::beginExternalRendering() {
    // Flush and invalidate everything
    endCurrentCommands();
    beginCurrentCommands();

    return m_cmd;
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
    this->endCurrentPass(true);

    auto mapping = util::resolveSrcComponentMapping(
      dstView->info().unpackSwizzle(),
      srcView->info().unpackSwizzle());

    // Use render pass path if we need format reinterpretation or if any
    // of the images are multisampled, which HW blit does not support.
    bool useFb = dstView->image()->info().sampleCount != VK_SAMPLE_COUNT_1_BIT
              || srcView->image()->info().sampleCount != VK_SAMPLE_COUNT_1_BIT
              || dstView->image()->info().format != dstView->info().format
              || srcView->image()->info().format != srcView->info().format
              || !util::isIdentityMapping(mapping);

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


  void DxvkContext::clearBuffer(
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          uint32_t              value) {
    DxvkResourceAccess access(*buffer, offset, length,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(
      DxvkCmdBuffer::InitBuffer, 1u, &access);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      endCurrentPass(true);

    syncResources(cmdBuffer, 1u, &access);

    auto bufferSlice = buffer->getSliceInfo(offset, align(length, sizeof(uint32_t)));

    if (length > sizeof(value)) {
      m_cmd->cmdFillBuffer(cmdBuffer,
        bufferSlice.buffer,
        bufferSlice.offset,
        bufferSlice.size,
        value);
    } else {
      m_cmd->cmdUpdateBuffer(cmdBuffer,
        bufferSlice.buffer,
        bufferSlice.offset,
        bufferSlice.size,
        &value);
    }
  }
  
  
  void DxvkContext::clearBufferView(
    const Rc<DxvkBufferView>&   bufferView,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          VkClearColorValue     value) {
    DxvkResourceAccess access(*bufferView, offset, length,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(
      DxvkCmdBuffer::InitBuffer, 1u, &access);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      endCurrentPass(true);
      invalidateState();
    }

    syncResources(cmdBuffer, 1u, &access);

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
    DxvkDescriptorWrite descriptor = { };
    descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    descriptor.descriptor = bufferView->getDescriptor(false);

    // Prepare shader arguments
    DxvkMetaClearArgs pushArgs = { };
    pushArgs.clearValue = value;
    pushArgs.offset = VkOffset3D {  int32_t(offset), 0, 0 };
    pushArgs.extent = VkExtent3D { uint32_t(length), 1, 1 };

    VkExtent3D workgroups = util::computeBlockCount(
      pushArgs.extent, pipeInfo.workgroupSize);

    m_cmd->cmdBindPipeline(cmdBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);

    m_cmd->bindResources(cmdBuffer, pipeInfo.layout,
      1u, &descriptor, sizeof(pushArgs), &pushArgs);

    m_cmd->cmdDispatch(cmdBuffer,
      workgroups.width, workgroups.height, workgroups.depth);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(cmdBuffer);
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
      // 3) The clear gets executed separately, in which case updateRenderTargets
      //    will indirectly emit barriers for the given render target.
      // If there is overlap, we need to explicitly transition affected attachments.
      this->endCurrentPass(true);
    } else if (!m_state.om.framebufferInfo.isWritable(attachmentIndex, clearAspects)) {
      // We cannot inline clears if the clear aspects are not writable. End the
      // render pass on the next draw to ensure that the image gets cleared.
      if (m_flags.test(DxvkContextFlag::GpRenderPassActive))
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
    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstBuffer, dstOffset, numBytes, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    accessBatch.emplace_back(*srcBuffer, srcOffset, numBytes, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(DxvkCmdBuffer::InitBuffer, accessBatch.size(), accessBatch.data());

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      this->endCurrentPass(true);

    syncResources(cmdBuffer, accessBatch.size(), accessBatch.data());

    auto srcSlice = srcBuffer->getSliceInfo(srcOffset, numBytes);
    auto dstSlice = dstBuffer->getSliceInfo(dstOffset, numBytes);

    VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copyRegion.srcOffset = srcSlice.offset;
    copyRegion.dstOffset = dstSlice.offset;
    copyRegion.size      = dstSlice.size;

    VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo.srcBuffer = srcSlice.buffer;
    copyInfo.dstBuffer = dstSlice.buffer;
    copyInfo.regionCount = 1;
    copyInfo.pRegions = &copyRegion;

    m_cmd->cmdCopyBuffer(cmdBuffer, &copyInfo);
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
    bool useFb = !formatsAreBufferCopyCompatible(dstImage->info().format, srcFormat)
              || dstImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;

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
    if (this->copyImageClear(dstImage, dstSubresource, dstOffset, extent, srcImage, srcSubresource))
      return;

    bool useFb = !formatsAreImageCopyCompatible(dstImage->info().format, srcImage->info().format);

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
    bool useFb = !formatsAreBufferCopyCompatible(srcImage->info().format, dstFormat);

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
    this->endCurrentPass(true);
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

      auto tmpBufferSlice = tmpBuffer->getSliceInfo();
      auto srcBufferSlice = srcView->getSliceInfo();

      viewInfo.offset = 0;
      auto tmpView = tmpBuffer->createView(viewInfo);

      small_vector<DxvkResourceAccess, 2u> accessBatch;
      accessBatch.emplace_back(*srcView, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
      accessBatch.emplace_back(*tmpView, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

      VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
      copyRegion.srcOffset = srcBufferSlice.offset;
      copyRegion.dstOffset = tmpBufferSlice.offset;
      copyRegion.size = tmpBufferSlice.size;

      VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
      copyInfo.srcBuffer = srcBufferSlice.buffer;
      copyInfo.dstBuffer = tmpBufferSlice.buffer;
      copyInfo.regionCount = 1;
      copyInfo.pRegions = &copyRegion;

      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &copyInfo);

      srcView = std::move(tmpView);
    }

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*srcView, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    accessBatch.emplace_back(*dstView, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    auto pipeInfo = m_common->metaCopy().getCopyFormattedBufferPipeline();

    std::array<DxvkDescriptorWrite, 2> descriptors = { };

    auto& dstDescriptor = descriptors[0u];
    dstDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    dstDescriptor.descriptor = dstView->getDescriptor(false);

    auto& srcDescriptor = descriptors[1u];
    srcDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    srcDescriptor.descriptor = srcView->getDescriptor(false);

    DxvkFormattedBufferCopyArgs args = { };
    args.dstOffset = dstOffset;
    args.srcOffset = srcOffset;
    args.extent = extent;
    args.dstSize = { dstSize.width, dstSize.height };
    args.srcSize = { srcSize.width, srcSize.height };

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeInfo.pipeline);

    m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.layout, descriptors.size(), descriptors.data(),
      sizeof(args), &args);

    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      (extent.width  + 7) / 8,
      (extent.height + 7) / 8,
      extent.depth);
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
    if (this->commitComputeState<false>()) {
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
    auto argInfo = m_state.id.argBuffer.getSliceInfo();

    if (this->commitComputeState<true>()) {
      m_queryManager.beginQueries(m_cmd,
        VK_QUERY_TYPE_PIPELINE_STATISTICS);

      m_cmd->cmdDispatchIndirect(DxvkCmdBuffer::ExecBuffer,
        argInfo.buffer, argInfo.offset + offset);
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
      auto argInfo = m_state.id.cntBuffer.getSliceInfo();

      m_cmd->cmdDrawIndirectVertexCount(1, 0,
        argInfo.buffer, argInfo.offset + counterOffset,
        counterBias, counterDivisor);
      m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

      // The count will generally be written from streamout
      if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
       || m_state.id.cntBuffer.buffer()->hasGfxStores())
        accessDrawCountBuffer(counterOffset);
    }
  }


  void DxvkContext::initBuffer(
    const Rc<DxvkBuffer>&           buffer) {
    auto dstSlice = buffer->getSliceInfo();

    // If the buffer is suballocated, clear the entire allocated
    // region, which is guaranteed to have a nicely aligned size
    if (!buffer->storage()->flags().test(DxvkAllocationFlag::OwnsBuffer)) {
      auto bufferInfo = buffer->storage()->getBufferInfo();

      dstSlice.buffer = bufferInfo.buffer;
      dstSlice.offset = bufferInfo.offset;
      dstSlice.size = bufferInfo.size;
    }

    // Buffer size may be misaligned, in which case we have
    // to use a plain buffer copy to fill the last few bytes.
    constexpr VkDeviceSize MinCopyAndFillSize = 1u << 20;

    VkDeviceSize copySize = dstSlice.size & 3u;
    VkDeviceSize fillSize = dstSlice.size - copySize;

    // If the buffer is small, just dispatch one single copy
    if (copySize && dstSlice.size < MinCopyAndFillSize) {
      copySize = dstSlice.size;
      fillSize = 0u;
    }

    if (fillSize) {
      m_cmd->cmdFillBuffer(DxvkCmdBuffer::SdmaBuffer,
        dstSlice.buffer, dstSlice.offset, fillSize, 0u);
    }

    if (copySize) {
      auto srcSlice = createZeroBuffer(copySize)->getSliceInfo();

      VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
      copyRegion.srcOffset = srcSlice.offset;
      copyRegion.dstOffset = dstSlice.offset + fillSize;
      copyRegion.size      = copySize;

      VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
      copyInfo.srcBuffer = srcSlice.buffer;
      copyInfo.dstBuffer = dstSlice.buffer;
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

          auto zeroSlice = createZeroBuffer(dataSize)->getSliceInfo();

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
              copyInfo.srcBuffer = zeroSlice.buffer;
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

    // The image will be in its default layout after this
    image->trackLayout(image->getAvailableSubresources(), image->info().layout);
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

    image->trackLayout(image->getAvailableSubresources(), image->info().layout);

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

    this->endCurrentPass(true);

    // Flush barriers if there was no active render pass.
    // This is necessary because there are no resources
    // associated with the barrier to allow tracking.
    if (srcStages | dstStages)
      flushBarriers();
  }


  void DxvkContext::acquireExternalResource(
      const Rc<DxvkPagedResource>&    resource,
            VkImageLayout             layout) {
    DxvkResourceAccess access;
    access.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    access.access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    access.buffer = dynamic_cast<DxvkBuffer*>(resource.ptr());
    access.image = dynamic_cast<DxvkImage*>(resource.ptr());

    if (access.buffer) {
      access.bufferOffset = 0u;
      access.bufferSize = access.buffer->info().size;
    } else if (access.image) {
      access.imageSubresources = access.image->getAvailableSubresources();
      access.imageLayout = layout;

      // Need to overwrite the tracked layout
      access.image->trackLayout(access.imageSubresources, access.imageLayout);
    }

    // Try to acquire on the init command buffer since this will generally
    // be the first use of the resource in a command list. Otherwise, we
    // need to flush barriers since there may be release barriers already.
    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(
      DxvkCmdBuffer::InitBarriers, 1u, &access);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      endCurrentPass(true);

    if (access.image && access.imageLayout != access.image->info().layout) {
      // External release barrier and layout transition in one go
      transitionImageLayout(*access.image, access.imageSubresources,
        access.stages, access.access, access.imageLayout,
        access.image->info().stages, access.image->info().access, false);
      flushImageLayoutTransitions(cmdBuffer);
    } else {
      releaseResources(cmdBuffer, 1u, &access);
    }

    m_cmd->track(resource, DxvkAccess::Read);
  }


  void DxvkContext::releaseExternalResource(
    const Rc<DxvkPagedResource>&    resource,
          VkImageLayout             layout) {
    endCurrentPass(true);

    DxvkResourceAccess access;
    access.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    access.access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    access.buffer = dynamic_cast<DxvkBuffer*>(resource.ptr());
    access.image = dynamic_cast<DxvkImage*>(resource.ptr());

    if (access.buffer) {
      access.bufferOffset = 0u;
      access.bufferSize = access.buffer->info().size;
    } else if (access.image) {
      access.imageSubresources = access.image->getAvailableSubresources();
      access.imageLayout = layout;
    }

    // Prepare resource for external use, hence acquire
    acquireResources(DxvkCmdBuffer::ExecBuffer, 1u, &access);
  }



  void DxvkContext::generateMipmaps(
    const Rc<DxvkImageView>&        imageView,
          VkFilter                  filter) {
    if (imageView->info().mipCount <= 1)
      return;
    
    this->endCurrentPass(true);

    // Check whether we can use the single-pass mip gen compute shader. Its main
    // advantage is that it does not require any internal synchronization as long
    // as only a single pass is required to process all mips in the view.
    bool useCs = filter == VK_FILTER_LINEAR
      && imageView->image()->info().type == VK_IMAGE_TYPE_2D
      && imageView->image()->info().sampleCount == VK_SAMPLE_COUNT_1_BIT
      && m_common->metaMipGen().checkFormatSupport(imageView->info().format);

    if (useCs) {
      DxvkImageUsageInfo usageInfo;
      usageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;

      useCs = ensureImageCompatibility(imageView->image(), usageInfo);
    }

    // If we can't use compute, use plain blits if the view format matches the image
    // format exactly. Beneficial on hardware that has dedicated 2D engines.
    bool useHw = !useCs
      && imageView->info().format == imageView->image()->info().format
      && imageView->image()->info().sampleCount == VK_SAMPLE_COUNT_1_BIT;

    if (useHw) {
      DxvkFormatFeatures formatFeatures = m_device->adapter()->getFormatFeatures(imageView->info().format);

      VkFormatFeatureFlags2 features = imageView->image()->info().tiling == VK_IMAGE_TILING_OPTIMAL
        ? formatFeatures.optimal
        : formatFeatures.linear;

      useHw = (features & VK_FORMAT_FEATURE_2_BLIT_DST_BIT)
           && (features & VK_FORMAT_FEATURE_2_BLIT_SRC_BIT);
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = imageView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, vk::makeLabel(0xe6dcf0,
        str::format("Mip gen (", dstName ? dstName : "unknown", ")").c_str()));
    }

    if (useCs)
      generateMipmapsCs(imageView);
    else if (useHw)
      generateMipmapsHw(imageView, filter);
    else
      generateMipmapsFb(imageView, filter);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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
          Rc<DxvkResourceAllocation>&& slice,
          VkImageLayout             layout) {
    invalidateImageWithUsage(image, std::move(slice), DxvkImageUsageInfo(), layout);
  }


  void DxvkContext::invalidateImageWithUsage(
    const Rc<DxvkImage>&            image,
          Rc<DxvkResourceAllocation>&& slice,
    const DxvkImageUsageInfo&       usageInfo,
          VkImageLayout             layout) {
    VkImageUsageFlags usage = image->info().usage;

    // Invalidate active image descriptors
    if (usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT))
      m_descriptorState.dirtyViews(image->getShaderStages());

    // Ensure that the image is in its default layout before invalidation
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      bool found = false;

      for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments() && !found; i++)
        found = m_state.om.framebufferInfo.getAttachment(i).view->image() == image;

      if (found) {
        m_flags.set(DxvkContextFlag::GpDirtyRenderTargets);

        endCurrentPass(true);
        flushDeferredClear(*image, image->getAvailableSubresources());
      }
    }

    // If there are any pending accesses that may involve
    // layout transitions, ensure that they are done
    if (resourceHasAccess(*image, image->getAvailableSubresources(), DxvkAccess::Write, DxvkAccessOp::None)) {
      endCurrentPass(true);
      flushBarriers();
    }

    // Ensure that the image is in its default layout. No need to explicitly
    // track it here though since we track the storage object instead.
    if (image->queryLayout(image->getAvailableSubresources()) != image->info().layout) {
      endCurrentPass(true);

      transitionImageLayout(*image,
        image->getAvailableSubresources(),
        image->info().stages, image->info().access,
        image->info().layout, image->info().stages, image->info().access, false);
      flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);
    }

    // Actually replace backing storage and make sure to keep the old one alive
    Rc<DxvkResourceAllocation> prevAllocation = image->assignStorageWithUsage(std::move(slice), usageInfo);
    m_cmd->track(std::move(prevAllocation));

    if (usageInfo.stableGpuAddress)
      m_common->memoryManager().lockResourceGpuAddress(image->storage());

    // If the image is new and uninitialized, submit a layout transition
    image->trackLayout(image->getAvailableSubresources(), layout);

    if (layout != image->info().layout) {
      if (layout == VK_IMAGE_LAYOUT_UNDEFINED || layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
        transitionImageLayout(*image, image->getAvailableSubresources(),
          image->info().stages, image->info().access, image->info().layout,
          image->info().stages, image->info().access, layout == VK_IMAGE_LAYOUT_UNDEFINED);
        flushImageLayoutTransitions(DxvkCmdBuffer::InitBarriers);

        m_cmd->track(image, DxvkAccess::Read);
      } else {
        // Track non-default layout as necessary
        m_nonDefaultLayoutImages.emplace_back(image);
      }
    }

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
    this->endCurrentPass(true);

    DxvkResourceAccess access(*image, image->getAvailableSubresources(),
      image->info().layout, image->info().stages, image->info().access, false);

    if (isUsageAndFormatCompatible && usageInfo.layout)
      access.imageLayout = usageInfo.layout;

    acquireResources(DxvkCmdBuffer::ExecBuffer, 1u, &access);

    if (isUsageAndFormatCompatible) {
      // Emit a barrier. If used in internal passes, this function
      // must be called *before* emitting dirty checks there.
      image->assignStorageWithUsage(image->storage(), usageInfo);

      if (usageInfo.stableGpuAddress)
        m_common->memoryManager().lockResourceGpuAddress(image->storage());
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
      auto argInfo = m_state.id.argBuffer.getSliceInfo();

      if (likely(count == 1u || !unroll || !needsDrawBarriers())) {
        if (Indexed) {
          m_cmd->cmdDrawIndexedIndirect(argInfo.buffer,
            argInfo.offset + offset, count, stride);
        } else {
          m_cmd->cmdDrawIndirect(argInfo.buffer,
            argInfo.offset + offset, count, stride);
        }

        m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

        if (unroll) {
          // Assume this is an automatically merged draw, if the app
          // explicitly uses multidraw then don't count it as merged.
          m_cmd->addStatCtr(DxvkStatCounter::CmdDrawsMerged, count - 1u);
        }

        if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
         || m_state.id.argBuffer.buffer()->hasGfxStores())
          accessDrawBuffer(offset, count, stride, elementSize);
      } else {
        // If the pipeline has order-sensitive stores, submit one
        // draw at a time and insert barriers in between.
        for (uint32_t i = 0; i < count; i++) {
          if (i)
            this->commitGraphicsState<Indexed, true>();

          if (Indexed) {
            m_cmd->cmdDrawIndexedIndirect(argInfo.buffer,
              argInfo.offset + offset, 1u, 0u);
          } else {
            m_cmd->cmdDrawIndirect(argInfo.buffer,
              argInfo.offset + offset, 1u, 0u);
          }

          if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
           || m_state.id.argBuffer.buffer()->hasGfxStores())
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
      auto argInfo = m_state.id.argBuffer.getSliceInfo();
      auto cntInfo = m_state.id.cntBuffer.getSliceInfo();

      if (Indexed) {
        m_cmd->cmdDrawIndexedIndirectCount(
          argInfo.buffer, argInfo.offset + offset,
          cntInfo.buffer, cntInfo.offset + countOffset,
          maxCount, stride);
      } else {
        m_cmd->cmdDrawIndirectCount(
          argInfo.buffer, argInfo.offset + offset,
          cntInfo.buffer, cntInfo.offset + countOffset,
          maxCount, stride);
      }

      m_cmd->addStatCtr(DxvkStatCounter::CmdDrawCalls, 1u);

      if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
       || m_state.id.argBuffer.buffer()->hasGfxStores()) {
        accessDrawBuffer(offset, maxCount, stride, Indexed
          ? sizeof(VkDrawIndexedIndirectCommand)
          : sizeof(VkDrawIndirectCommand));
      }

      if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
       || m_state.id.cntBuffer.buffer()->hasGfxStores())
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

    this->endCurrentPass(true);

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
        region, format, mode, stencilMode, true);
    }
  }


  void DxvkContext::transformImage(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceRange&  dstSubresources,
          VkImageLayout             srcLayout,
          VkImageLayout             dstLayout) {
    this->endCurrentPass(false);

    if (srcLayout != dstLayout) {
      DxvkResourceAccess access(*dstImage, dstSubresources,
        dstLayout, dstImage->info().stages, dstImage->info().access,
        srcLayout == VK_IMAGE_LAYOUT_UNDEFINED);

      acquireResources(DxvkCmdBuffer::ExecBuffer, 1u, &access);
    }
  }
  
  
  VkAttachmentStoreOp DxvkContext::determineClearStoreOp(
          VkAttachmentLoadOp        loadOp) const {
    if (loadOp == VK_ATTACHMENT_LOAD_OP_NONE)
      return VK_ATTACHMENT_STORE_OP_NONE;

    if (loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;

    return VK_ATTACHMENT_STORE_OP_STORE;
  }


  std::optional<DxvkClearInfo> DxvkContext::batchClear(
    const Rc<DxvkImageView>&        imageView,
          int32_t                   attachmentIndex,
          VkImageAspectFlags        discardAspects,
          VkImageAspectFlags        clearAspects,
          VkClearValue              clearValue) {
    bool hasLoadOpNone = m_device->properties().khrMaintenance7.separateDepthStencilAttachmentAccess;

    DxvkColorAttachmentOps colorOp;
    colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    
    DxvkDepthAttachmentOps depthOp;
    depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_LOAD;

    if (clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      colorOp.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    
    if (clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    else if (hasLoadOpNone)
      depthOp.loadOpD = VK_ATTACHMENT_LOAD_OP_NONE;

    if (clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_CLEAR;
    else if (discardAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    else if (hasLoadOpNone)
      depthOp.loadOpS = VK_ATTACHMENT_LOAD_OP_NONE;

    if (attachmentIndex >= 0 && !m_state.om.framebufferInfo.isWritable(attachmentIndex, clearAspects | discardAspects)) {
      // Do not fold the clear/discard into the render pass if any of the affected aspects
      // isn't writable. We can only hit this particular path when starting a render pass,
      // so we can safely manipulate load layouts here.
      attachmentIndex = -1;
    }

    // Completely ignore pure discards here if we can't fold them into the next
    // render pass, since all we'd do is add an extra barrier for no reason.
    if (attachmentIndex < 0 && !clearAspects)
      return std::nullopt;

    if (attachmentIndex < 0) {
      std::optional<DxvkClearInfo> result;

      auto& clearInfo = result.emplace();
      clearInfo.view = imageView;
      clearInfo.loadOp = colorOp.loadOp;
      clearInfo.clearValue = clearValue;
      clearInfo.clearAspects = clearAspects;
      clearInfo.discardAspects = discardAspects;

      if ((clearAspects | discardAspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        clearInfo.loadOp = depthOp.loadOpD;
        clearInfo.loadOpS = depthOp.loadOpS;
      }

      return result;
    } else {
      // Perform the operation when starting the next render pass
      if ((clearAspects | discardAspects) & VK_IMAGE_ASPECT_COLOR_BIT) {
        uint32_t colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

        m_state.om.renderPassOps.colorOps[colorIndex].loadOp = colorOp.loadOp;
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

      return std::nullopt;
    }
  }


  void DxvkContext::performClears(
    const DxvkClearBatch&           batch) {
    auto [entries, count] = batch.getRange();

    // Batch barriers and try to hoist clears if possible
    small_vector<DxvkResourceAccess, 16> accessBatch;

    for (size_t i = 0u; i < count; i++) {
      const auto& entry = entries[i];

      VkPipelineStageFlagBits2 clearStages = 0u;
      VkAccessFlags2 clearAccess = 0u;

      if ((entry.clearAspects | entry.discardAspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        clearStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        clearAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                    |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      } else {
        clearStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        clearAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                    |  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
      }

      accessBatch.emplace_back(*entry.view, clearStages, clearAccess,
        (entry.clearAspects | entry.discardAspects) == entry.view->info().aspects);
    }

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(DxvkCmdBuffer::InitBuffer,
      accessBatch.size(), accessBatch.data());

    syncResources(cmdBuffer, accessBatch.size(), accessBatch.data(), false);

    // Execute clears
    for (size_t i = 0u; i < count; i++) {
      const auto& entry = entries[i];

      bool useLateClear = m_device->perfHints().renderPassClearFormatBug
        && entry.view->info().format != entry.view->image()->info().format;

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
        const char* imageName = entry.view->image()->info().debugName;
        m_cmd->cmdBeginDebugUtilsLabel(cmdBuffer,
          vk::makeLabel(0xe6f0dc, str::format("Clear render target (", imageName ? imageName : "unknown", ")").c_str()));
      }

      // Set up a temporary render pass to execute the clear
      VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
      attachmentInfo.imageView = entry.view->handle();
      attachmentInfo.imageLayout = entry.view->getLayout();
      attachmentInfo.clearValue = entry.clearValue;

      VkRenderingAttachmentInfo stencilInfo = attachmentInfo;

      VkExtent3D extent = entry.view->mipLevelExtent(0);

      VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
      renderingInfo.renderArea.extent = { extent.width, extent.height };
      renderingInfo.layerCount = entry.view->info().layerCount;

      if ((entry.clearAspects | entry.discardAspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        if (entry.view->info().aspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
          renderingInfo.pDepthAttachment = &attachmentInfo;

          attachmentInfo.loadOp = entry.loadOp;
          attachmentInfo.storeOp = determineClearStoreOp(entry.loadOp);
        }

        if (entry.view->info().aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
          renderingInfo.pStencilAttachment = &stencilInfo;

          stencilInfo.loadOp = entry.loadOpS;
          stencilInfo.storeOp = determineClearStoreOp(entry.loadOpS);
        }
      } else {
        attachmentInfo.loadOp = entry.loadOp;
        attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if (useLateClear && entry.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
          attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;
      }

      m_cmd->cmdBeginRendering(cmdBuffer, &renderingInfo);

      if (useLateClear) {
        VkClearAttachment clearInfo = { };
        clearInfo.aspectMask = entry.clearAspects;
        clearInfo.clearValue = entry.clearValue;

        VkClearRect clearRect = { };
        clearRect.rect = renderingInfo.renderArea;
        clearRect.layerCount = renderingInfo.layerCount;

        m_cmd->cmdClearAttachments(cmdBuffer, 1, &clearInfo, 1, &clearRect);
      }

      m_cmd->cmdEndRendering(cmdBuffer);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        m_cmd->cmdEndDebugUtilsLabel(cmdBuffer);
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
        this->endCurrentPass(false);
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
        this->endCurrentPass(false);
        break;
      }
    }

    m_deferredClears.push_back({ imageView, discardAspects });
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

      m_cmd->cmdClearAttachments(DxvkCmdBuffer::ExecBuffer,
        attachments.size(), attachments.data(), 1u, &clearRect);

      // Full clears require the render area to cover everything
      m_state.om.renderAreaLo = VkOffset2D { 0, 0 };
      m_state.om.renderAreaHi = VkOffset2D {
        int32_t(clearRect.rect.extent.width),
        int32_t(clearRect.rect.extent.height) };
    }

    m_deferredClears.clear();
  }


  void DxvkContext::flushClears(
          bool                      useRenderPass) {
    DxvkClearBatch clearBatch;

    for (const auto& clear : m_deferredClears) {
      int32_t attachmentIndex = -1;

      if (useRenderPass && m_state.om.framebufferInfo.isFullSize(clear.imageView))
        attachmentIndex = m_state.om.framebufferInfo.findAttachment(clear.imageView);

      clearBatch.add(batchClear(clear.imageView, attachmentIndex,
        clear.discardAspects, clear.clearAspects, clear.clearValue));
    }

    m_deferredClears.clear();

    if (!clearBatch.empty())
      performClears(clearBatch);
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

      VkImageLayout newLayout = dstImage->pickLayout(isDepthStencil
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      bool isFullWrite = (resolve.depthMode || !(dstSubresource.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT))
                      && (resolve.stencilMode || !(dstSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT));

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

      // We never make MSAA passes unsynchronized, so this should be fine
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

      transitionImageLayout(*dstImage, dstSubresource,
        dstImage->info().stages, dstImage->info().access, newLayout, stages, access,
        isFullWrite);

      // Record post-resolve barrier. This is not a layout transition.
      accessImage(DxvkCmdBuffer::ExecBuffer, *dstImage, dstSubresource,
        newLayout, stages, access,
        newLayout, dstImage->info().stages, dstImage->info().access,
        DxvkAccessOp::None);

      if (!isDepthStencil) {
        uint32_t index = m_state.om.framebufferInfo.getColorAttachmentIndex(i);

        auto& color = m_state.om.renderingInfo.color[index];
        color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color.resolveImageView = resolve.imageView->handle();
        color.resolveImageLayout = newLayout;

        auto& flags = m_state.om.renderingInfo.colorAttachmentFlags[index];
        flags.flags |= resolve.flags;
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

        // We're within a render pass, any pending clears will have
        // happened after the resolve, so ignore them here.
        resolveImageRp(resolve.imageView->image(), attachment.view->image(),
          region, attachment.view->info().format, resolve.depthMode, resolve.stencilMode, false);

        resolve.layerMask &= ~0u << (layerIndex + layerCount);
      }

      // Reset deferred resolve state
      resolve = DxvkDeferredResolve();
    }
  }


  void DxvkContext::finalizeLoadStoreOps() {
    auto& renderingInfo = m_state.om.renderingInfo;

    // Track attachment access for render pass clears and resolves
    bool hasClearOrResolve = false;

    for (uint32_t i = 0; i < renderingInfo.rendering.colorAttachmentCount; i++) {
      if (renderingInfo.color[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_state.om.attachmentMask.trackColorWrite(i);
        hasClearOrResolve = true;
      }

      if (renderingInfo.color[i].resolveImageView && renderingInfo.color[i].resolveMode) {
        m_state.om.attachmentMask.trackColorRead(i);
        hasClearOrResolve = true;
      }
    }

    if (renderingInfo.rendering.pDepthAttachment) {
      if (renderingInfo.depth.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_state.om.attachmentMask.trackDepthWrite();
        hasClearOrResolve = true;
      }

      if (renderingInfo.depth.resolveImageView && renderingInfo.depth.resolveMode) {
        m_state.om.attachmentMask.trackDepthRead();
        hasClearOrResolve = true;
      }
    }

    if (renderingInfo.rendering.pStencilAttachment) {
      if (renderingInfo.stencil.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_state.om.attachmentMask.trackStencilWrite();
        hasClearOrResolve = true;
      }

      if (renderingInfo.stencil.resolveImageView && renderingInfo.stencil.resolveMode) {
        m_state.om.attachmentMask.trackStencilRead();
        hasClearOrResolve = true;
      }
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

    // If we can prove that the app has only rendered to a portion of
    // the image, adjust the render area to the exact rendered region.
    if (!hasClearOrResolve && m_state.om.renderAreaLo.x < m_state.om.renderAreaHi.x
                           && m_state.om.renderAreaLo.y < m_state.om.renderAreaHi.y) {
      renderingInfo.rendering.renderArea.offset = m_state.om.renderAreaLo;
      renderingInfo.rendering.renderArea.extent = VkExtent2D {
        uint32_t(m_state.om.renderAreaHi.x - m_state.om.renderAreaLo.x),
        uint32_t(m_state.om.renderAreaHi.y - m_state.om.renderAreaLo.y) };
    }
  }


  void DxvkContext::adjustAttachmentLoadStoreOps(
          VkRenderingAttachmentInfo&  attachment,
          DxvkAccess                  access) const {
    if (access == DxvkAccess::None) {
      // If the attachment is not accessed at all, we can set both the
      // load and store op to NONE if supported by the implementation.
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_NONE;
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
    DxvkResourceAccess access(*buffer, offset, size,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(DxvkCmdBuffer::InitBuffer, 1u, &access);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      endCurrentPass(true);

    syncResources(cmdBuffer, 1u, &access);

    auto bufferSlice = buffer->getSliceInfo(offset, size);

    m_cmd->cmdUpdateBuffer(cmdBuffer,
      bufferSlice.buffer,
      bufferSlice.offset,
      bufferSlice.size,
      data);
  }
  
  
  void DxvkContext::uploadBuffer(
    const Rc<DxvkBuffer>&           buffer,
    const Rc<DxvkBuffer>&           source,
          VkDeviceSize              sourceOffset) {
    auto bufferSlice = buffer->getSliceInfo();
    auto sourceSlice = source->getSliceInfo(sourceOffset, buffer->info().size);

    VkBufferCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copyRegion.srcOffset = sourceSlice.offset;
    copyRegion.dstOffset = bufferSlice.offset;
    copyRegion.size      = bufferSlice.size;

    VkCopyBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo.srcBuffer = sourceSlice.buffer;
    copyInfo.dstBuffer = bufferSlice.buffer;
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
    bool useFb = !formatsAreBufferCopyCompatible(image->info().format, format)
              || image->info().sampleCount != VK_SAMPLE_COUNT_1_BIT;

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
      m_flags.set(DxvkContextFlag::GpDirtySampleLocations);

      if (!m_state.gp.state.ms.sampleCount())
        m_flags.set(DxvkContextFlag::GpDirtyMultisampleState);

      if (!m_features.test(DxvkContextFeature::VariableMultisampleRate))
        m_flags.set(DxvkContextFlag::GpRenderPassNeedsFlush);
    }

    DxvkRsInfo rsInfo(
      rs.depthClip(),
      rs.polygonMode(),
      rs.sampleCount(),
      rs.conservativeMode(),
      rs.flatShading(),
      rs.lineMode());

    if (!m_state.gp.state.rs.eq(rsInfo)) {
      m_flags.set(DxvkContextFlag::GpDirtyPipelineState,
                  DxvkContextFlag::GpDirtyDepthClip);
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
    if (m_state.dyn.depthStencilState.depthTest() != ds.depthTest()
     || m_state.dyn.depthStencilState.depthWrite() != ds.depthWrite()
     || m_state.dyn.depthStencilState.depthCompareOp() != ds.depthCompareOp())
      m_flags.set(DxvkContextFlag::GpDirtyDepthTest);

    if (m_state.dyn.depthStencilState.stencilTest() != ds.stencilTest()
     || !m_state.dyn.depthStencilState.stencilOpFront().eq(ds.stencilOpFront())
     || !m_state.dyn.depthStencilState.stencilOpBack().eq(ds.stencilOpBack()))
      m_flags.set(DxvkContextFlag::GpDirtyStencilTest);

    m_state.dyn.depthStencilState = ds;
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
    DxvkBarrierControlFlags mask = m_flags.test(DxvkContextFlag::GpRenderPassActive)
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
    this->endCurrentPass(true);
    
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
    this->endCurrentPass(true);

    std::vector<DxvkResourceAccess> accessBatch;

    for (auto& r : buffers) {
      accessBatch.emplace_back(*r.first, 0u, r.first->info().size,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }

    for (auto& r : images) {
      accessBatch.emplace_back(*r.first, r.first->getAvailableSubresources(), r.first->info().layout,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, false);
    }

    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    if (m_features.test(DxvkContextFeature::DescriptorHeap)) {
      // HACK: We want to use the driver-managed descriptor heap here, but
      // there is no spec-legal way to bind it, not even by starting a new
      // command buffer. For now, work around the problem by calling a
      // legacy descriptor set function.
      m_cmd->cmdBindDescriptorSets(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, VK_NULL_HANDLE, 0, 0, nullptr);
      m_cmd->invalidateDescriptorHeapBinding();
    }

    m_cmd->cmdLaunchCuKernel(nvxLaunchInfo);
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

    bool hasFeedbackLoop = false;
    bool hasColorAttachments = false;
    bool hasDepthAttachment = m_state.om.renderTargets.depth.view != nullptr;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      hasColorAttachments |= m_state.om.renderTargets.color[i].view != nullptr;

    std::stringstream label;

    if (hasColorAttachments == hasDepthAttachment)
      label << "Render";
    else if (hasColorAttachments)
      label << "Color";
    else if (hasDepthAttachment)
      label << "Depth";

    label << " pass " << uint32_t(++m_renderPassIndex) << " (";

    hasColorAttachments = false;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_state.om.renderTargets.color[i].view) {
        const char* imageName = m_state.om.renderTargets.color[i].view->image()->info().debugName;
        label << (hasColorAttachments ? ", " : "") << i << ": " << (imageName ? imageName : "unknown");

        hasColorAttachments = true;
        sampleCount = m_state.om.renderTargets.color[i].view->image()->info().sampleCount;

        hasFeedbackLoop = hasFeedbackLoop ||
          (m_state.om.renderTargets.color[i].view->image()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT);
      }
    }

    if (m_state.om.renderTargets.depth.view) {
      if (hasColorAttachments)
        label << ", ";

      const char* imageName = m_state.om.renderTargets.depth.view->image()->info().debugName;
      label << "DS:" << (imageName ? imageName : "unknown");

      sampleCount = m_state.om.renderTargets.depth.view->image()->info().sampleCount;

      hasFeedbackLoop = hasFeedbackLoop ||
        (m_state.om.renderTargets.depth.view->image()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT);
    }

    if (!hasColorAttachments && !hasDepthAttachment)
      label << "No attachments";

    if (sampleCount > VK_SAMPLE_COUNT_1_BIT)
      label << ", " << uint32_t(sampleCount) << "x MSAA";

    label << ")";

    uint32_t color = sampleCount > VK_SAMPLE_COUNT_1_BIT ? 0xf0dcf0 : 0xf0e6dc;

    if (hasFeedbackLoop)
      color = 0xdceff0;

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

    bool dstIsDepthStencil = dstView->info().aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    dstView = ensureImageViewCompatibility(dstView, dstIsDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    srcView = ensureImageViewCompatibility(srcView, VK_IMAGE_USAGE_SAMPLED_BIT);

    if (!dstView || !srcView) {
      Logger::err(str::format("DxvkContext: blitImageFb: Resources not supported"));
      return;
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstView->image()->info().debugName;
      const char* srcName = srcView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Blit (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstView, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, false);
    accessBatch.emplace_back(*srcView, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_ACCESS_2_SHADER_READ_BIT, false);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

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

    // Determine resolve mode for when the source is multisampled. If
    // there is no stretching going on, do a regular resolve.
    auto resolveMode = filter == VK_FILTER_NEAREST
      ? DxvkMetaBlitResolveMode::FilterNearest
      : DxvkMetaBlitResolveMode::FilterLinear;

    if (srcView->image()->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      bool isSameExtent = (std::abs(dstOffsets[1].x - dstOffsets[0].x) == std::abs(srcOffsets[1].x - srcOffsets[0].x))
                       && (std::abs(dstOffsets[1].y - dstOffsets[0].y) == std::abs(srcOffsets[1].y - srcOffsets[0].y))
                       && (std::abs(dstOffsets[1].z - dstOffsets[0].z) == std::abs(srcOffsets[1].z - srcOffsets[0].z));

      if (isSameExtent)
        resolveMode = DxvkMetaBlitResolveMode::ResolveAverage;
    }

    DxvkMetaBlitPipeline pipeInfo = m_common->metaBlit().getPipeline(
      dstView->info().viewType, dstView->info().format,
      srcView->image()->info().sampleCount,
      dstView->image()->info().sampleCount, resolveMode);

    VkViewport viewport = { };
    viewport.x = float(dstOffsetsAdjusted[0].x);
    viewport.y = float(dstOffsetsAdjusted[0].y);
    viewport.width = float(dstExtent.width);
    viewport.height = float(dstExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { };
    scissor.offset = { dstOffsetsAdjusted[0].x, dstOffsetsAdjusted[0].y  };
    scissor.extent = { dstExtent.width, dstExtent.height };

    // Begin render pass
    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = dstView->handle();
    attachmentInfo.imageLayout = dstView->getLayout();
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea = scissor;
    renderingInfo.layerCount = dstView->info().layerCount;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);
    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    // Set up source image view
    Rc<DxvkSampler> sampler = createBlitSampler(filter);

    DxvkDescriptorWrite imageDescriptor = { };
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageDescriptor.descriptor = srcView->getDescriptor();
    
    // Compute shader parameters for the operation
    VkExtent3D srcExtent = srcView->mipLevelExtent(0);

    DxvkMetaBlitPushConstants pushConstants = { };
    pushConstants.layerCount = dstView->info().layerCount;
    pushConstants.sampler = sampler->getDescriptor().samplerIndex;
    if (srcView->image()->info().sampleCount == VK_SAMPLE_COUNT_1_BIT) {
      pushConstants.srcCoord0 = {
        float(srcOffsetsAdjusted[0].x) / float(srcExtent.width),
        float(srcOffsetsAdjusted[0].y) / float(srcExtent.height),
        float(srcOffsetsAdjusted[0].z) / float(srcExtent.depth) };
      pushConstants.srcCoord1 = {
        float(srcOffsetsAdjusted[1].x) / float(srcExtent.width),
        float(srcOffsetsAdjusted[1].y) / float(srcExtent.height),
        float(srcOffsetsAdjusted[1].z) / float(srcExtent.depth) };
    } else {
      // Src coords are in texels rather than 0.0 - 1.0
      pushConstants.srcCoord0 = {
        float(srcOffsetsAdjusted[0].x),
        float(srcOffsetsAdjusted[0].y),
        float(srcOffsetsAdjusted[0].z) };
      pushConstants.srcCoord1 = {
        float(srcOffsetsAdjusted[1].x),
        float(srcOffsetsAdjusted[1].y),
        float(srcOffsetsAdjusted[1].z) };
    }

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeline);

    m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.layout, 1u, &imageDescriptor,
      sizeof(pushConstants), &pushConstants);

    m_cmd->cmdDraw(3, pushConstants.layerCount, 0, 0);
    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    m_cmd->track(std::move(sampler));
  }


  void DxvkContext::blitImageHw(
    const Rc<DxvkImageView>&    dstView,
    const VkOffset3D*           dstOffsets,
    const Rc<DxvkImageView>&    srcView,
    const VkOffset3D*           srcOffsets,
          VkFilter              filter) {
    // Prepare the two images for transfer ops if necessary
    auto dstLayout = dstView->image()->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    auto srcLayout = srcView->image()->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstView->image(), dstView->imageSubresources(),
      dstLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, false);
    accessBatch.emplace_back(*srcView->image(), srcView->imageSubresources(),
      srcLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

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
  }


  template<bool ToImage>
  void DxvkContext::copyImageBufferData(
          DxvkCmdBuffer         cmd,
    const Rc<DxvkImage>&        image,
    const VkImageSubresourceLayers& imageSubresource,
          VkOffset3D            imageOffset,
          VkExtent3D            imageExtent,
          VkImageLayout         imageLayout,
    const DxvkResourceBufferInfo& bufferSlice,
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
          copyInfo.srcBuffer = bufferSlice.buffer;
          copyInfo.dstImage = image->handle();
          copyInfo.dstImageLayout = imageLayout;
          copyInfo.regionCount = 1;
          copyInfo.pRegions = &copyRegion;

          m_cmd->cmdCopyBufferToImage(cmd, &copyInfo);
        } else {
          VkCopyImageToBufferInfo2 copyInfo = { VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2 };
          copyInfo.srcImage = image->handle();
          copyInfo.srcImageLayout = imageLayout;
          copyInfo.dstBuffer = bufferSlice.buffer;
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
    VkDeviceSize dataSize = imageSubresource.layerCount * util::computeImageDataSize(
      image->info().format, imageExtent, imageSubresource.aspectMask);

    // We may copy to only one aspect at a time, but pipeline
    // barriers need to have all available aspect bits set
    auto dstFormatInfo = image->formatInfo();
    auto dstLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    auto dstSubresource = vk::makeSubresourceRange(imageSubresource);
    dstSubresource.aspectMask = dstFormatInfo->aspectMask;

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*buffer, bufferOffset, dataSize,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

    auto& imageAccess = accessBatch.emplace_back(*image, dstSubresource, dstLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      image->isFullSubresource(imageSubresource, imageExtent));
    imageAccess.imageOffset = imageOffset;
    imageAccess.imageExtent = imageExtent;

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(DxvkCmdBuffer::InitBuffer,
      accessBatch.size(), accessBatch.data());

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      endCurrentPass(true);

    syncResources(cmdBuffer, accessBatch.size(), accessBatch.data());

    auto bufferSlice = buffer->getSliceInfo(bufferOffset, dataSize);

    copyImageBufferData<true>(cmdBuffer,
      image, imageSubresource, imageOffset, imageExtent, dstLayout,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);
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
    this->endCurrentPass(true);
    this->invalidateState();

    bool isDepthStencil = imageSubresource.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    // Ensure we can read the source image
    DxvkImageUsageInfo imageUsage = { };
    imageUsage.usage = isDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (!ensureImageCompatibility(image, imageUsage)) {
      Logger::err(str::format("DxvkContext: copyBufferToImageFb: Unsupported image:"
        "\n  format: ", image->info().format));
    }

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

    // Create image view to render to
    bool discard = image->isFullSubresource(imageSubresource, imageExtent);

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

    VkPipelineStageFlags stages = isDepthStencil
      ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
      : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkAccessFlags access = isDepthStencil
      ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
      : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*bufferView, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    accessBatch.emplace_back(*imageView, stages, access, discard);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    // Bind image for rendering
    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = imageView->handle();
    attachment.imageLayout = imageView->getLayout();
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // Don't bother optimizing load ops for the partial copy case,
    // that should basically never happen to begin with
    if (imageSubresource.aspectMask != image->formatInfo()->aspectMask)
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    VkViewport viewport = { };
    viewport.x = imageOffset.x;
    viewport.y = imageOffset.y;
    viewport.width = imageExtent.width;
    viewport.height = imageExtent.height;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { };
    scissor.offset = { imageOffset.x, imageOffset.y };
    scissor.extent = { imageExtent.width, imageExtent.height };

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea = scissor;
    renderingInfo.layerCount = imageViewInfo.layerCount;

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &attachment;
    }

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      renderingInfo.pDepthAttachment = &attachment;

    if (image->formatInfo()->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      renderingInfo.pStencilAttachment = &attachment;

    m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);

    // Set up viewport and scissor state
    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    // Get pipeline and descriptor set layout. All pipelines
    // will be using the same pipeline layout here.
    bool needsBitwiseStencilCopy = !m_device->features().extShaderStencilExport
      && (imageSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT);

    // If we have a depth aspect, this will give us either the depth-only
    // pipeline or one that can write all the given aspects
    DxvkMetaCopyPipeline pipeline = m_common->metaCopy().getCopyBufferToImagePipeline(
      image->info().format, bufferFormat, imageSubresource.aspectMask,
      image->info().sampleCount);

    DxvkDescriptorWrite bufferDescriptor = { };
    bufferDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    bufferDescriptor.descriptor = bufferView->getDescriptor(false);

    DxvkBufferImageCopyArgs pushConst = { };
    pushConst.imageOffset = imageOffset;
    pushConst.bufferOffset = 0u;
    pushConst.imageExtent = imageExtent;
    pushConst.bufferImageWidth = rowPitch / formatInfo->elementSize;
    pushConst.bufferImageHeight = slicePitch / rowPitch;

    if (imageSubresource.aspectMask != VK_IMAGE_ASPECT_STENCIL_BIT || !needsBitwiseStencilCopy) {
      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

      m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
        pipeline.layout, 1u, &bufferDescriptor,
        sizeof(pushConst), &pushConst);

      m_cmd->cmdDraw(3, renderingInfo.layerCount, 0, 0);
    }

    if (needsBitwiseStencilCopy) {
      // On systems that do not support stencil export, we need to clear
      // stencil to 0 and then "write" each individual bit by discarding
      // fragments where that bit is not set.
      pipeline = m_common->metaCopy().getCopyBufferToImagePipeline(
        image->info().format, bufferFormat, VK_IMAGE_ASPECT_STENCIL_BIT,
        image->info().sampleCount);

      if (imageSubresource.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) {
        VkClearAttachment clear = { };
        clear.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

        VkClearRect clearRect = { };
        clearRect.rect = scissor;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = renderingInfo.layerCount;

        m_cmd->cmdClearAttachments(DxvkCmdBuffer::ExecBuffer, 1, &clear, 1, &clearRect);
      }

      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

      for (uint32_t i = 0; i < 8; i++) {
        pushConst.stencilBitIndex = i;

        m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
          pipeline.layout, i ? 0u : 1u, &bufferDescriptor,
          sizeof(pushConst), &pushConst);

        m_cmd->cmdSetStencilWriteMask(VK_STENCIL_FACE_FRONT_AND_BACK, 1u << i);
        m_cmd->cmdDraw(3, renderingInfo.layerCount, 0, 0);
      }
    }

    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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
    this->endCurrentPass(true);

    VkDeviceSize dataSize = imageSubresource.layerCount * util::computeImageDataSize(
      image->info().format, imageExtent, imageSubresource.aspectMask);

    auto bufferSlice = buffer->getSliceInfo(bufferOffset, dataSize);

    // We may copy to only one aspect of a depth-stencil image,
    // but pipeline barriers need to have all aspect bits set
    auto srcFormatInfo = image->formatInfo();

    auto srcSubresource = imageSubresource;
    srcSubresource.aspectMask = srcFormatInfo->aspectMask;

    // Select a suitable image layout for the transfer op
    VkImageLayout srcLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*buffer, bufferOffset, dataSize,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    auto& dstAccess = accessBatch.emplace_back(*image,
      vk::makeSubresourceRange(srcSubresource), srcLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    dstAccess.imageOffset = imageOffset;
    dstAccess.imageExtent = imageExtent;

    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    this->copyImageBufferData<false>(DxvkCmdBuffer::ExecBuffer,
      image, imageSubresource, imageOffset, imageExtent, srcLayout,
      bufferSlice, bufferRowAlignment, bufferSliceAlignment);
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
    this->endCurrentPass(true);
    this->invalidateState();

    // Ensure we can read the source image
    DxvkImageUsageInfo imageUsage = { };
    imageUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    ensureImageCompatibility(image, imageUsage);

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

    // Transition image to a layout we can use for reading as necessary
    VkImageLayout imageLayout = (image->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? image->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      : image->pickLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*bufferView, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    auto& srcAccess = accessBatch.emplace_back(*image, vk::makeSubresourceRange(imageSubresource),
      imageLayout, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    srcAccess.imageOffset = imageOffset;
    srcAccess.imageExtent = imageExtent;

    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    // Retrieve pipeline
    VkImageViewType viewType = image->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
      : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    if (image->info().type == VK_IMAGE_TYPE_3D)
      viewType = VK_IMAGE_VIEW_TYPE_3D;

    DxvkMetaCopyPipeline pipeline = m_common->metaCopy().getCopyImageToBufferPipeline(viewType, bufferFormat);

    // Create image views  for the main and stencil aspects
    std::array<DxvkDescriptorWrite, 3> descriptors = { };

    DxvkImageViewKey imageViewInfo;
    imageViewInfo.viewType = viewType;
    imageViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageViewInfo.format = image->info().format;
    imageViewInfo.layout = imageLayout;
    imageViewInfo.mipIndex = imageSubresource.mipLevel;
    imageViewInfo.mipCount = 1;
    imageViewInfo.layerIndex = imageSubresource.baseArrayLayer;
    imageViewInfo.layerCount = imageSubresource.layerCount;

    auto& bufferDescriptor = descriptors[0u];
    bufferDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    bufferDescriptor.descriptor = bufferView->getDescriptor(false);

    auto& imagePlane0Descriptor = descriptors[1u];
    imagePlane0Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if ((imageViewInfo.aspects = (imageSubresource.aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_PLANE_0_BIT))))
      imagePlane0Descriptor.descriptor = image->createView(imageViewInfo)->getDescriptor();

    auto& imagePlane1Descriptor = descriptors[2u];
    imagePlane1Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if ((imageViewInfo.aspects = (imageSubresource.aspectMask & (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT))))
      imagePlane1Descriptor.descriptor = image->createView(imageViewInfo)->getDescriptor();

    // Compute number of workgroups
    VkExtent3D workgroupCount = imageExtent;
    workgroupCount.depth *= imageSubresource.layerCount;
    workgroupCount = util::computeBlockCount(workgroupCount, { 16, 16, 1 });

    // Set up shader arguments and dispatch shader
    DxvkBufferImageCopyArgs pushConst = { };
    pushConst.imageOffset = imageOffset;
    pushConst.bufferOffset = 0u;
    pushConst.imageExtent = imageExtent;
    pushConst.bufferImageWidth = rowPitch / formatInfo->elementSize;
    pushConst.bufferImageHeight = slicePitch / rowPitch;

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

    m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
      pipeline.layout, descriptors.size(), descriptors.data(),
      sizeof(pushConst), &pushConst);

    m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      workgroupCount.width,
      workgroupCount.height,
      workgroupCount.depth);

    m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
}


  void DxvkContext::clearImageViewFb(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkImageAspectFlags    aspect,
          VkClearValue          value) {
    // Ensure that the clear region is actually within the bounds of the given view
    VkExtent3D mipExtent = imageView->mipLevelExtent(0);

    offset.x = std::clamp(offset.x, 0, int32_t(mipExtent.width));
    offset.y = std::clamp(offset.y, 0, int32_t(mipExtent.height));

    extent.width  = std::clamp(extent.width,  0u, mipExtent.width  - uint32_t(offset.x));
    extent.height = std::clamp(extent.height, 0u, mipExtent.height - uint32_t(offset.y));

    if (unlikely(!extent.width || !extent.height))
      return;

    // Use regular render target clear path if we're clearing the
    // entire view to hit some additional optimizations.
    if (extent == imageView->mipLevelExtent(0u)) {
      clearRenderTarget(imageView, aspect, value, 0u);
      return;
    }

    // Find out if the render target view is currently bound, so that
    // we can avoid spilling the render pass if it is. Don't try to
    // use the render pass if the framebuffer size is too small.
    this->updateRenderTargets();

    int32_t attachmentIndex = -1;

    DxvkFramebufferSize fbSize = m_state.om.framebufferInfo.size();

    if (uint32_t(offset.x) + extent.width <= fbSize.width
     && uint32_t(offset.y) + extent.height <= fbSize.height)
      attachmentIndex = m_state.om.framebufferInfo.findAttachment(imageView);

    if (attachmentIndex >= 0 && !m_state.om.framebufferInfo.isWritable(attachmentIndex, aspect))
      attachmentIndex = -1;

    if (attachmentIndex < 0) {
      this->endCurrentPass(true);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
        const char* dstName = imageView->image()->info().debugName;

        m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer, vk::makeLabel(0xf0dcdc,
          str::format("Clear view (", dstName ? dstName : "unknown", ")").c_str()));
      }

      // Use render area to limit the actual clear region
      VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
      attachmentInfo.imageView = imageView->handle();
      attachmentInfo.imageLayout = imageView->getLayout();
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentInfo.clearValue = value;

      VkRenderingAttachmentInfo stencilInfo = attachmentInfo;

      VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
      renderingInfo.renderArea.offset = { offset.x,     offset.y };
      renderingInfo.renderArea.extent = { extent.width, extent.height };
      renderingInfo.layerCount = imageView->info().layerCount;

      VkPipelineStageFlags clearStages = 0;
      VkAccessFlags clearAccess = 0;

      if (imageView->info().aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        clearStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        clearAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        if (imageView->info().aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
          renderingInfo.pDepthAttachment = &attachmentInfo;

        if (imageView->info().aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
          renderingInfo.pStencilAttachment = &stencilInfo;

        if (imageView->info().aspects != aspect) {
          if (m_device->properties().khrMaintenance7.separateDepthStencilAttachmentAccess) {
            if (!(aspect & VK_IMAGE_ASPECT_DEPTH_BIT)) {
              attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_NONE;
              attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
            }

            if (!(aspect & VK_IMAGE_ASPECT_STENCIL_BIT)) {
              stencilInfo.loadOp = VK_ATTACHMENT_LOAD_OP_NONE;
              stencilInfo.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
            }
          } else {
            clearAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

            if (!(aspect & VK_IMAGE_ASPECT_DEPTH_BIT)) {
              attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
              attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            }

            if (!(aspect & VK_IMAGE_ASPECT_STENCIL_BIT)) {
              stencilInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
              stencilInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            }
          }
        }
      } else {
        clearStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        clearAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;
      }

      DxvkResourceAccess access(*imageView, clearStages, clearAccess, false);
      syncResources(DxvkCmdBuffer::ExecBuffer, 1u, &access);

      m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);
      m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
    } else {
      // Make sure the render pass is active so
      // that we can actually perform the clear
      this->beginRenderPass();

      if (findOverlappingDeferredClear(*imageView->image(), imageView->imageSubresources()))
        flushClearsInline();

      // Inline clears may affect render area
      m_state.om.renderAreaLo = VkOffset2D {
        std::min(m_state.om.renderAreaLo.x, offset.x),
        std::min(m_state.om.renderAreaLo.y, offset.y) };
      m_state.om.renderAreaHi = VkOffset2D {
        std::max(m_state.om.renderAreaHi.x, int32_t(offset.x + extent.width)),
        std::max(m_state.om.renderAreaHi.y, int32_t(offset.y + extent.height)) };

      // Check whether we can fold the clear into the curret render pass. This is the
      // case when the framebuffer size matches the clear size, even if the clear itself
      // does not cover the entire view, and if the view has not been accessed yet.
      bool hoistClear = m_flags.test(DxvkContextFlag::GpRenderPassSecondaryCmd)
        && extent.width == fbSize.width && extent.height == fbSize.height;

      DxvkDeferredClear clearInfo = { };
      clearInfo.imageView = imageView;
      clearInfo.clearAspects = aspect;
      clearInfo.clearValue = value;

      uint32_t colorIndex = 0u;

      if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
        colorIndex = m_state.om.framebufferInfo.getColorAttachmentIndex(attachmentIndex);

        if (hoistClear && m_state.om.attachmentMask.getColorAccess(colorIndex) == DxvkAccess::None)
          hoistInlineClear(clearInfo, m_state.om.renderingInfo.color[colorIndex], VK_IMAGE_ASPECT_COLOR_BIT);
      } else {
        if (hoistClear && m_state.om.attachmentMask.getDepthAccess() == DxvkAccess::None)
          hoistInlineClear(clearInfo, m_state.om.renderingInfo.depth, VK_IMAGE_ASPECT_DEPTH_BIT);

        if (hoistClear && m_state.om.attachmentMask.getStencilAccess() == DxvkAccess::None)
          hoistInlineClear(clearInfo, m_state.om.renderingInfo.stencil, VK_IMAGE_ASPECT_STENCIL_BIT);
      }

      // If we hoisted everything, there's nothing left to do
      if (!clearInfo.clearAspects)
        return;

      if (clearInfo.clearAspects & VK_IMAGE_ASPECT_COLOR_BIT)
        m_state.om.attachmentMask.trackColorWrite(colorIndex);

      if (clearInfo.clearAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        m_state.om.attachmentMask.trackDepthWrite();

      if (clearInfo.clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        m_state.om.attachmentMask.trackStencilWrite();

      // Perform the actual clear operation
      VkClearAttachment clear = { };
      clear.aspectMask = aspect;
      clear.colorAttachment = colorIndex;
      clear.clearValue = value;

      VkClearRect clearRect = { };
      clearRect.rect.offset.x = offset.x;
      clearRect.rect.offset.y = offset.y;
      clearRect.rect.extent.width = extent.width;
      clearRect.rect.extent.height = extent.height;
      clearRect.layerCount = imageView->info().layerCount;

      m_cmd->cmdClearAttachments(DxvkCmdBuffer::ExecBuffer, 1, &clear, 1, &clearRect);
    }
  }

  
  void DxvkContext::clearImageViewCs(
    const Rc<DxvkImageView>&    imageView,
          VkOffset3D            offset,
          VkExtent3D            extent,
          VkClearValue          value) {
    DxvkResourceAccess access(*imageView, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
      imageView->image()->isFullSubresource(vk::pickSubresourceLayers(imageView->imageSubresources(), 0u), extent));

    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(DxvkCmdBuffer::InitBuffer, 1u, &access);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      endCurrentPass(true);
      invalidateState();
    }

    syncResources(cmdBuffer, 1u, &access);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = imageView->image()->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(cmdBuffer, vk::makeLabel(0xf0dcdc,
        str::format("Clear view (", dstName ? dstName : "unknown", ")").c_str()));
    }

    // Query pipeline objects to use for this clear operation
    DxvkMetaClearPipeline pipeInfo = m_common->metaClear().getClearImagePipeline(
      imageView->type(), lookupFormatInfo(imageView->info().format)->flags);

    // Create a descriptor set pointing to the view
    DxvkDescriptorWrite imageDescriptor = { };
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageDescriptor.descriptor = imageView->getDescriptor();
    
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

    m_cmd->bindResources(cmdBuffer, pipeInfo.layout,
      1u, &imageDescriptor, sizeof(pushArgs), &pushArgs);

    m_cmd->cmdDispatch(cmdBuffer,
      workgroups.width, workgroups.height, workgroups.depth);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(cmdBuffer);
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

    auto srcFormatInfo = srcImage->formatInfo();
    auto dstFormatInfo = dstImage->formatInfo();

    // If we copy between disjoint regions of the same image subresources,
    // make sure that we only do one single transition to GENERAL.
    bool hasOvelap = dstImage == srcImage && vk::checkSubresourceRangeOverlap(dstSubresourceRange, srcSubresourceRange);

    small_vector<DxvkResourceAccess, 2u> accessBatch;

    VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    if (hasOvelap) {
      VkImageSubresourceRange overlapSubresourceRange = { };
      overlapSubresourceRange.aspectMask = srcSubresource.aspectMask | dstSubresource.aspectMask;
      overlapSubresourceRange.baseArrayLayer = std::min(srcSubresource.baseArrayLayer, dstSubresource.baseArrayLayer);
      overlapSubresourceRange.baseMipLevel = srcSubresource.mipLevel;
      overlapSubresourceRange.layerCount = std::max(srcSubresource.baseArrayLayer, dstSubresource.baseArrayLayer)
        + dstSubresource.layerCount - overlapSubresourceRange.baseArrayLayer;
      overlapSubresourceRange.levelCount = 1u;

      accessBatch.emplace_back(*dstImage, overlapSubresourceRange, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, false);
    } else {
      dstImageLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      srcImageLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

      auto& dstAccess = accessBatch.emplace_back(*dstImage, dstSubresourceRange, dstImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        dstImage->isFullSubresource(dstSubresource, extent));
      dstAccess.imageOffset = srcOffset;
      dstAccess.imageExtent = extent;

      auto& srcAccess = accessBatch.emplace_back(*srcImage, srcSubresourceRange, srcImageLayout,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
      srcAccess.imageOffset = srcOffset;
      srcAccess.imageExtent = extent;
    }

    // Try to do the copy out of order to avoid barrier spam
    DxvkCmdBuffer cmdBuffer = prepareOutOfOrderTransfer(
      DxvkCmdBuffer::InitBuffer, accessBatch.size(), accessBatch.data());

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer)
      this->endCurrentPass(true);

    syncResources(cmdBuffer, accessBatch.size(), accessBatch.data());

    auto dstAspects = dstSubresource.aspectMask;
    auto srcAspects = srcSubresource.aspectMask;

    while (dstAspects && srcAspects) {
      VkImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
      copyRegion.srcSubresource = srcSubresource;
      copyRegion.srcSubresource.aspectMask = vk::getNextAspect(srcAspects);
      copyRegion.srcOffset = srcOffset;
      copyRegion.dstSubresource = dstSubresource;
      copyRegion.dstSubresource.aspectMask = vk::getNextAspect(dstAspects);
      copyRegion.dstOffset = dstOffset;
      copyRegion.extent = extent;

      if (srcFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
        auto plane = &srcFormatInfo->planes[vk::getPlaneIndex(copyRegion.srcSubresource.aspectMask)];
        copyRegion.srcOffset.x /= plane->blockSize.width;
        copyRegion.srcOffset.y /= plane->blockSize.height;
      }

      if (dstFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
        auto plane = &dstFormatInfo->planes[vk::getPlaneIndex(copyRegion.dstSubresource.aspectMask)];
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

      m_cmd->cmdCopyImage(cmdBuffer, &copyInfo);
    }
  }

  
  void DxvkContext::copyImageFb(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource,
          VkOffset3D            srcOffset,
          VkExtent3D            extent) {
    this->endCurrentPass(true);

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

    // Create source and destination image views
    DxvkMetaCopyViews views(
      dstImage, dstSubresource, viewFormats.dstFormat,
      srcImage, srcSubresource, viewFormats.srcFormat);

    VkAccessFlags dstAccess = views.srcImageView->getLayout();
    VkImageLayout dstLayout = views.dstImageView->getLayout();

    // Flag used to determine whether we can do an UNDEFINED transition
    bool doDiscard = dstImage->isFullSubresource(dstSubresource, extent);

    // This function can process both color and depth-stencil images, so
    // some things change a lot depending on the destination image type
    VkPipelineStageFlags dstStages;

    if (dstSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
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

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstImage, dstSubresourceRange, dstLayout,
      dstStages, dstAccess, doDiscard);
    accessBatch.emplace_back(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    // Create pipeline for the copy operation
    DxvkMetaCopyPipeline pipeInfo = m_common->metaCopy().getCopyImagePipeline(
      views.srcImageView->info().viewType, viewFormats.dstFormat, dstImage->info().sampleCount);

    // Create and initialize descriptor set
    std::array<DxvkDescriptorWrite, 2> descriptors = { };

    auto& imagePlane0Descriptor = descriptors[0u];
    imagePlane0Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imagePlane0Descriptor.descriptor = views.srcImageView->getDescriptor();

    auto& imagePlane1Descriptor = descriptors[1u];
    imagePlane1Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if (views.srcStencilView)
      imagePlane1Descriptor.descriptor = views.srcStencilView->getDescriptor();

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

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = views.dstImageView->handle();
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // Don't bother optimizing load ops for the partial copy case, shouldn't happen
    if (dstSubresource.aspectMask != dstImage->formatInfo()->aspectMask)
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea = scissor;
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

    VkOffset2D srcCoordOffset = {
      srcOffset.x - dstOffset.x,
      srcOffset.y - dstOffset.y };

    // Perform the actual copy operation
    m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);

    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeline);

    m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.layout, descriptors.size(), descriptors.data(),
      sizeof(srcCoordOffset), &srcCoordOffset);

    m_cmd->cmdDraw(3, dstSubresource.layerCount, 0, 0);
    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
  }


  bool DxvkContext::copyImageClear(
    const Rc<DxvkImage>&        dstImage,
          VkImageSubresourceLayers dstSubresource,
          VkOffset3D            dstOffset,
          VkExtent3D            dstExtent,
    const Rc<DxvkImage>&        srcImage,
          VkImageSubresourceLayers srcSubresource) {
    this->endCurrentPass(true);

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
    const DxvkDeferredClear* clear = findDeferredClear(*srcImage, vk::makeSubresourceRange(srcSubresource));

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

    auto sparseSlice = sparse->getSliceInfo();
    auto bufferSlice = buffer->getSliceInfo(offset, SparseMemoryPageSize * pageCount);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*sparse, 0u, sparseSlice.size, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT);
    accessBatch.emplace_back(*buffer, offset, bufferSlice.size, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    for (uint32_t i = 0; i < pageCount; i++) {
      auto pageInfo = pageTable->getPageInfo(pages[i]);

      if (pageInfo.type == DxvkSparsePageType::Buffer) {
        VkDeviceSize sparseOffset = pageInfo.buffer.offset;
        VkDeviceSize bufferOffset = bufferSlice.offset + SparseMemoryPageSize * i;

        VkBufferCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
        copy.srcOffset = ToBuffer ? sparseOffset : bufferOffset;
        copy.dstOffset = ToBuffer ? bufferOffset : sparseOffset;
        copy.size = pageInfo.buffer.length;

        regions.push_back(copy);
      }
    }

    VkCopyBufferInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    info.srcBuffer = ToBuffer ? sparseSlice.buffer: bufferSlice.buffer;
    info.dstBuffer = ToBuffer ? bufferSlice.buffer: sparseSlice.buffer;
    info.regionCount = uint32_t(regions.size());
    info.pRegions = regions.data();

    if (info.regionCount)
      m_cmd->cmdCopyBuffer(DxvkCmdBuffer::ExecBuffer, &info);
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

    auto bufferSlice = buffer->getSliceInfo(offset, SparseMemoryPageSize * pageCount);

    VkImageLayout transferLayout = sparse->pickLayout(ToBuffer
      ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
      : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*sparse, sparse->getAvailableSubresources(), transferLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, ToBuffer ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT, false);
    accessBatch.emplace_back(*buffer, offset, bufferSlice.size, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      ToBuffer ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    for (uint32_t i = 0; i < pageCount; i++) {
      auto pageInfo = pageTable->getPageInfo(pages[i]);

      if (pageInfo.type == DxvkSparsePageType::Image) {
        VkBufferImageCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
        copy.bufferOffset = bufferSlice.offset + SparseMemoryPageSize * i;
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
      info.dstBuffer = bufferSlice.buffer;
      info.regionCount = regions.size();
      info.pRegions = regions.data();

      if (info.regionCount)
        m_cmd->cmdCopyImageToBuffer(DxvkCmdBuffer::ExecBuffer, &info);
    } else {
      VkCopyBufferToImageInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
      info.srcBuffer = bufferSlice.buffer;
      info.dstImage = sparse->handle();
      info.dstImageLayout = transferLayout;
      info.regionCount = regions.size();
      info.pRegions = regions.data();

      if (info.regionCount)
        m_cmd->cmdCopyBufferToImage(DxvkCmdBuffer::ExecBuffer, &info);
    }
  }


  void DxvkContext::generateMipmapsHw(
    const Rc<DxvkImageView>&        imageView,
          VkFilter                  filter) {
    auto image = imageView->image();

    VkImageSubresourceRange subresources = imageView->imageSubresources();

    VkImageSubresourceRange srcSubresource = subresources;
    srcSubresource.levelCount = 1u;

    VkImageSubresourceRange dstSubresource = subresources;
    dstSubresource.baseMipLevel += 1u;
    dstSubresource.levelCount -= 1u;

    VkImageLayout srcLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkImageLayout dstLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    for (uint32_t i = 0u; i < subresources.levelCount - 1u; i++) {
      // Transition and discard all levels that will be written in the first iteration,
      // then just synchronize access as normal. Requires that the destination mip count
      // is left intact for the first iteration.
      small_vector<DxvkResourceAccess, 2u> accessBatch;
      accessBatch.emplace_back(*image, srcSubresource, srcLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
      accessBatch.emplace_back(*image, dstSubresource, dstLayout, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, !i);
      syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

      dstSubresource.levelCount = 1u;

      VkExtent3D dstSize = image->mipLevelExtent(dstSubresource.baseMipLevel);
      VkExtent3D srcSize = image->mipLevelExtent(srcSubresource.baseMipLevel);

      VkImageBlit2 blit = { VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
      blit.dstSubresource = vk::pickSubresourceLayers(dstSubresource, 0u);
      blit.srcSubresource = vk::pickSubresourceLayers(srcSubresource, 0u);
      blit.dstOffsets[1u] = { int32_t(dstSize.width), int32_t(dstSize.height), int32_t(dstSize.depth) };
      blit.srcOffsets[1u] = { int32_t(srcSize.width), int32_t(srcSize.height), int32_t(srcSize.depth) };

      VkBlitImageInfo2 info = { VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
      info.dstImage = image->handle();
      info.dstImageLayout = dstLayout;
      info.srcImage = image->handle();
      info.srcImageLayout = srcLayout;
      info.regionCount = 1u;
      info.pRegions = &blit;
      info.filter = filter;

      m_cmd->cmdBlitImage(&info);

      srcSubresource.baseMipLevel += 1u;
      dstSubresource.baseMipLevel += 1u;
    }
  }


  void DxvkContext::generateMipmapsFb(
    const Rc<DxvkImageView>&        imageView,
          VkFilter                  filter) {
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

    // Create image views, etc.
    DxvkMetaMipGenViews mipGenerator(imageView, VK_PIPELINE_BIND_POINT_GRAPHICS);

    // Common descriptor set properties that we use to
    // bind the source image view to the fragment shader
    Rc<DxvkSampler> sampler = createBlitSampler(filter);

    DxvkDescriptorWrite imageDescriptor = { };
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    // Retrieve a compatible pipeline to use for rendering
    auto resolveMode = filter == VK_FILTER_NEAREST
      ? DxvkMetaBlitResolveMode::FilterNearest
      : DxvkMetaBlitResolveMode::FilterLinear;

    DxvkMetaBlitPipeline pipeInfo = m_common->metaBlit().getPipeline(
      mipGenerator.getSrcViewType(), imageView->info().format,
      VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_1_BIT, resolveMode);

    for (uint32_t i = 0; i < mipGenerator.getPassCount(); i++) {
      auto srcView = mipGenerator.getSrcView(i);
      auto dstView = mipGenerator.getDstView(i);

      imageDescriptor.descriptor = srcView->getDescriptor();

      // Width, height and layer count for the current pass
      VkExtent3D passExtent = mipGenerator.computePassExtent(i);

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
      VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
      attachmentInfo.imageView = dstView->handle();
      attachmentInfo.imageLayout = dstView->getLayout();
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
      renderingInfo.renderArea = scissor;
      renderingInfo.layerCount = passExtent.depth;
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.pColorAttachments = &attachmentInfo;

      // Set up push constants
      DxvkMetaBlitPushConstants pushConstants = { };
      pushConstants.srcCoord0  = { 0.0f, 0.0f, 0.0f };
      pushConstants.srcCoord1  = { 1.0f, 1.0f, 1.0f };
      pushConstants.layerCount = passExtent.depth;
      pushConstants.sampler = sampler->getDescriptor().samplerIndex;

      // Emit per-subresource barriers
      small_vector<DxvkResourceAccess, 2u> accessBatch;
      accessBatch.emplace_back(*srcView, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
      accessBatch.emplace_back(*dstView,  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, true);
      syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

      m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);

      m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeline);

      m_cmd->cmdSetViewport(1, &viewport);
      m_cmd->cmdSetScissor(1, &scissor);

      m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
        pipeInfo.layout, 1u, &imageDescriptor,
        sizeof(pushConstants), &pushConstants);

      m_cmd->cmdDraw(3, passExtent.depth, 0, 0);
      m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);
    }

    m_cmd->track(std::move(sampler));
  }


  void DxvkContext::generateMipmapsCs(
    const Rc<DxvkImageView>&        imageView) {
    this->invalidateState();

    // Number of descriptors to bind, first is for source mip
    constexpr uint32_t MaxDescriptorCount = DxvkMetaMipGenObjects::MipCount * 2u + 1u;

    // Maximum image dimension that the mip gen shader can handle in on
    // pass. If the image is any larger, we need to emit extra passes.
    constexpr uint32_t MaxSinglePassSize = (2u << (DxvkMetaMipGenObjects::MipCount * 2u)) - 1u;

    // Linear filtering only with the current implementation
    Rc<DxvkSampler> sampler = createBlitSampler(VK_FILTER_LINEAR);

    // Create views. These are actually going to be usable even in a two-pass
    // scenario since the src view is *always* sampled rather than storage.
    DxvkMetaMipGenViews mipGenerator(imageView, VK_PIPELINE_BIND_POINT_COMPUTE);

    uint32_t layerCount = imageView->info().layerCount;

    // Check if we need to run two passes. This is only going to be the case if
    // the view's top level is larger than the largest mip would be in an image
    // where we downsample *twice* the per-step mip count, because the shader can
    // read back one mip and process it in the last running workgroup. However,
    // this only works on the actual mip tail of the base image, regardless of
    // which views are accessed, so we also need to make sure that the pre-pass
    // only processes a single batch of mips.
    VkExtent3D baseExtent = imageView->mipLevelExtent(0u);

    bool requiresPrepass = std::max(baseExtent.width, baseExtent.height) > MaxSinglePassSize
      && mipGenerator.getPassCount() > DxvkMetaMipGenObjects::MipCount;

    // Allocate scratch memory for per-layer workgroup counters and zero it on
    // the out-of-order init command buffer to avoid having to synchronize
    uint32_t counterDwordCount = layerCount * (requiresPrepass ? 2u : 1u);

    DxvkResourceBufferInfo counterMemory = allocateScratchMemory(
      sizeof(uint32_t), sizeof(uint32_t) * counterDwordCount);

    m_cmd->cmdFillBuffer(DxvkCmdBuffer::InitBuffer,
      counterMemory.buffer, counterMemory.offset, counterMemory.size, 0u);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

    m_initBarriers.addMemoryBarrier(barrier);

    // Bind pipeline already, no reason to do it twice if we need a pre-pass
    auto pipeline = m_common->metaMipGen().getPipeline(imageView->info().format);

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

    uint32_t basePass = 0u;

    while (basePass < mipGenerator.getPassCount()) {
      small_vector<DxvkResourceAccess, MaxDescriptorCount> accessBatch;

      DxvkMetaMipGenPushConstants pushData = { };
      pushData.atomicCounterVa = counterMemory.gpuAddress;
      pushData.samplerIndex = sampler->getDescriptor().samplerIndex;

      if (requiresPrepass && !basePass) {
        // It is beneficial to run this on the maximum mip count here because
        // sampling the top-level mip tends to be the primary bottleneck. We
        // already ensured that all these views are included anyway.
        pushData.mipCount = DxvkMetaMipGenObjects::MipCount;
      } else {
        // If we can, just do a single pass.
        pushData.mipCount = mipGenerator.getPassCount() - basePass;
      }

      // Bind views only for this pass, set null descriptors for unused mips.
      std::array<DxvkDescriptorWrite, MaxDescriptorCount> descriptors = { };

      size_t n = 0u;

      auto& descriptor = descriptors.at(n++);
      descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      descriptor.descriptor = mipGenerator.getSrcView(basePass)->getDescriptor();

      accessBatch.emplace_back(*mipGenerator.getSrcView(basePass),
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);

      for (uint32_t i = 0u; i < pushData.mipCount; i++) {
        auto& descriptor = descriptors.at(n++);
        descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor.descriptor = mipGenerator.getDstView(basePass + i)->getDescriptor();

        // Don't actually bother discarding mips here, there's *probably*
        // no good reason to introduce an extra sync point just for that.
        accessBatch.emplace_back(*mipGenerator.getDstView(basePass + i),
          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT, false);
      }

      while (n < descriptors.size()) {
        auto& descriptor = descriptors.at(n++);
        descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      }

      m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
        pipeline.layout, descriptors.size(), descriptors.data(),
        sizeof(pushData), &pushData);

      syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

      // Dispatch size must always match that of the bottom mip of one
      // pass, even if that mip is not included in the view at all
      VkExtent3D dispatchSize = imageView->mipLevelExtent(DxvkMetaMipGenObjects::MipCount);

      m_cmd->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
        dispatchSize.width, dispatchSize.height, layerCount);

      // Advance some stuff for the next pass
      basePass += pushData.mipCount;

      pushData.atomicCounterVa += layerCount * sizeof(uint32_t);
    }

    m_cmd->track(std::move(sampler));
  }


  void DxvkContext::resolveImageHw(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    // We only support resolving to the entire image
    // area, so we might as well discard its contents
    VkImageLayout dstLayout = dstImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageLayout srcLayout = srcImage->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstImage, dstSubresourceRange, dstLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      dstImage->isFullSubresource(region.dstSubresource, region.extent));
    accessBatch.emplace_back(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, false);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    VkResolveImageModeInfoKHR resolveMode = { VK_STRUCTURE_TYPE_RESOLVE_IMAGE_MODE_INFO_KHR };

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

    if (srcImage->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
     && m_device->features().khrMaintenance10.maintenance10
     && m_device->properties().khrMaintenance10.resolveSrgbFormatSupportsTransferFunctionControl) {
      resolveMode.pNext = std::exchange(resolveInfo.pNext, &resolveMode);
      resolveMode.flags = VK_RESOLVE_IMAGE_ENABLE_TRANSFER_FUNCTION_BIT_KHR;
    }

    m_cmd->cmdResolveImage(&resolveInfo);
  }


  void DxvkContext::resolveImageRp(
    const Rc<DxvkImage>&            dstImage,
    const Rc<DxvkImage>&            srcImage,
    const VkImageResolve&           region,
          VkFormat                  format,
          VkResolveModeFlagBits     mode,
          VkResolveModeFlagBits     stencilMode,
          bool                      flushClears) {
    auto dstSubresourceRange = vk::makeSubresourceRange(region.dstSubresource);
    auto srcSubresourceRange = vk::makeSubresourceRange(region.srcSubresource);

    bool isDepthStencil = (dstImage->formatInfo()->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

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

    // For some reason, someone somewhere decided that even depth-stencil
    // resolves are performed with COLOR_ATTACHMENT stage and access
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkAccessFlags2 srcAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    VkAccessFlags2 dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*srcImage, srcSubresourceRange, srcLayout, stages, srcAccess, false);
    accessBatch.emplace_back(*dstImage, dstSubresourceRange, dstLayout, stages, dstAccess, true);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data(), flushClears);

    // Create a pair of views for the attachment resolve
    DxvkMetaResolveViews views(dstImage, region.dstSubresource,
      srcImage, region.srcSubresource, format);

    VkRenderingAttachmentFlagsInfoKHR attachmentFlags = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_FLAGS_INFO_KHR };

    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = views.srcView->handle();
    attachment.imageLayout = srcLayout;
    attachment.resolveMode = mode;
    attachment.resolveImageView = views.dstView->handle();
    attachment.resolveImageLayout = dstLayout;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;

    if (srcImage->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
     && m_device->features().khrMaintenance10.maintenance10
     && m_device->properties().khrMaintenance10.resolveSrgbFormatSupportsTransferFunctionControl) {
      attachmentFlags.pNext = std::exchange(attachment.pNext, &attachmentFlags);
      attachmentFlags.flags = VK_RENDERING_ATTACHMENT_RESOLVE_ENABLE_TRANSFER_FUNCTION_BIT_KHR;
    }

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

    m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);
    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      const char* dstName = dstImage->info().debugName;
      const char* srcName = srcImage->info().debugName;

      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dcdc, str::format("Resolve (",
          dstName ? dstName : "unknown", ", ",
          srcName ? srcName : "unknown", ")").c_str()));
    }

    // Create image views for the resolve operation
    VkFormat dstFormat = format ? format : dstImage->info().format;
    VkFormat srcFormat = format ? format : srcImage->info().format;

    DxvkMetaCopyViews views(
      dstImage, region.dstSubresource, dstFormat,
      srcImage, region.srcSubresource, srcFormat);

    // Discard the destination image if we're fully writing it,
    // and transition the image layout if necessary
    VkImageLayout dstLayout = views.dstImageView->getLayout();
    VkImageLayout srcLayout = views.srcImageView->getLayout();

    bool doDiscard = dstImage->isFullSubresource(region.dstSubresource, region.extent);

    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      doDiscard &= depthMode != VK_RESOLVE_MODE_NONE;
    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      doDiscard &= stencilMode != VK_RESOLVE_MODE_NONE;

    VkPipelineStageFlags dstStages;
    VkAccessFlags dstAccess;

    if (region.dstSubresource.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
      dstStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (!doDiscard)
        dstAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    small_vector<DxvkResourceAccess, 2u> accessBatch;
    accessBatch.emplace_back(*dstImage, dstSubresourceRange, dstLayout, dstStages, dstAccess, doDiscard);
    accessBatch.emplace_back(*srcImage, srcSubresourceRange, srcLayout,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, false);
    syncResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    // Create a framebuffer and pipeline for the resolve op
    DxvkMetaResolvePipeline pipeInfo = m_common->metaResolve().getPipeline(
      dstFormat, srcImage->info().sampleCount, depthMode, stencilMode);

    // Create and initialize descriptor set
    std::array<DxvkDescriptorWrite, 2> descriptors = { };

    auto& imagePlane0Descriptor = descriptors[0u];
    imagePlane0Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imagePlane0Descriptor.descriptor = views.srcImageView->getDescriptor();

    auto& imagePlane1Descriptor = descriptors[1u];
    imagePlane1Descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if (views.srcStencilView)
      imagePlane1Descriptor.descriptor = views.srcStencilView->getDescriptor();

    // Set up render state    
    VkViewport viewport = { };
    viewport.x        = float(region.dstOffset.x);
    viewport.y        = float(region.dstOffset.y);
    viewport.width    = float(region.extent.width);
    viewport.height   = float(region.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { };
    scissor.offset = { region.dstOffset.x,  region.dstOffset.y   };
    scissor.extent = { region.extent.width, region.extent.height };

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = views.dstImageView->handle();
    attachmentInfo.imageLayout = dstLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea = scissor;
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
    
    m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);

    m_cmd->cmdSetViewport(1, &viewport);
    m_cmd->cmdSetScissor(1, &scissor);

    m_cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeInfo.pipeline);

    m_cmd->bindResources(DxvkCmdBuffer::ExecBuffer,
      pipeInfo.layout, descriptors.size(), descriptors.data(),
      sizeof(srcOffset), &srcOffset);

    m_cmd->cmdDraw(3, region.dstSubresource.layerCount, 0, 0);

    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      m_cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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
    const DxvkDeferredClear* clear = findDeferredClear(*srcImage, vk::makeSubresourceRange(region.srcSubresource));

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

    VkImageUsageFlagBits usageBit = isDepthStencil
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    DxvkImageUsageInfo usage = { };
    usage.usage = usageBit;
    usage.viewFormatCount = 1;
    usage.viewFormats = &format;

    if (!ensureImageCompatibility(dstImage, usage))
      return false;

    // End current render pass and prepare the destination image
    endCurrentPass(true);

    // Create an image view that we can use to perform the clear
    DxvkImageViewKey key = { };
    key.viewType = VK_IMAGE_VIEW_TYPE_2D;
    key.usage = usageBit;
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
    if (findOverlappingDeferredClear(*srcImage, vk::makeSubresourceRange(region.srcSubresource)))
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
    dstKey.usage = VkImageUsageFlagBits(usage);
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

    // Ensure that the resolve happens in linear space only if the resolve format is sRGB
    if (dstView->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
     && m_device->properties().khrMaintenance10.resolveSrgbFormatSupportsTransferFunctionControl) {
      resolve.flags = lookupFormatInfo(format)->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
        ? VK_RENDERING_ATTACHMENT_RESOLVE_ENABLE_TRANSFER_FUNCTION_BIT_KHR
        : VK_RENDERING_ATTACHMENT_RESOLVE_SKIP_TRANSFER_FUNCTION_BIT_KHR;
    }

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
          source->getSliceInfo(dataOffset, mipSize), 0, 0);

        dataOffset += align(mipSize, subresourceAlignment);
      }
    }

    accessImageTransfer(*image, image->getAvailableSubresources(), transferLayout,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    image->trackLayout(image->getAvailableSubresources(), image->info().layout);

    m_cmd->track(source, DxvkAccess::Read);
    m_cmd->track(image, DxvkAccess::Write);
  }


  void DxvkContext::beginRenderPass() {
    if (!m_flags.test(DxvkContextFlag::GpRenderPassActive)) {
      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl);

      prepareShaderReadableImages(true);

      // Make sure all graphics state gets reapplied on the next draw
      m_descriptorState.clearStages(VK_SHADER_STAGE_COMPUTE_BIT);
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

      m_flags.set(
        DxvkContextFlag::GpRenderPassActive,
        DxvkContextFlag::GpDirtyPipelineState,
        DxvkContextFlag::GpDirtyVertexBuffers,
        DxvkContextFlag::GpDirtyIndexBuffer,
        DxvkContextFlag::GpDirtyXfbBuffers,
        DxvkContextFlag::GpDirtyBlendConstants,
        DxvkContextFlag::GpDirtyStencilTest,
        DxvkContextFlag::GpDirtyStencilRef,
        DxvkContextFlag::GpDirtyMultisampleState,
        DxvkContextFlag::GpDirtyRasterizerState,
        DxvkContextFlag::GpDirtySampleLocations,
        DxvkContextFlag::GpDirtyViewport,
        DxvkContextFlag::GpDirtyDepthBias,
        DxvkContextFlag::GpDirtyDepthBounds,
        DxvkContextFlag::GpDirtyDepthClip,
        DxvkContextFlag::GpDirtyDepthTest);

      m_flags.clr(
        DxvkContextFlag::GpRenderPassSuspended,
        DxvkContextFlag::GpIndependentSets);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        beginRenderPassDebugRegion();

      this->renderPassBindFramebuffer(
        m_state.om.framebufferInfo,
        m_state.om.renderPassOps);

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
  
  
  void DxvkContext::endRenderPass(bool suspend) {
    if (m_flags.test(DxvkContextFlag::GpRenderPassActive)) {
      bool unsynchronized = m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized);

      if (unsynchronized) {
        // Do not allow any graphics shader hazards across render passes
        m_flags.set(DxvkContextFlag::ForceWriteAfterWriteSync);
      }

      m_flags.clr(DxvkContextFlag::GpRenderPassActive,
                  DxvkContextFlag::GpRenderPassSideEffects,
                  DxvkContextFlag::GpRenderPassNeedsFlush,
                  DxvkContextFlag::GpRenderPassUnsynchronized);

      this->pauseTransformFeedback();

      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_OCCLUSION);
      m_queryManager.endQueries(m_cmd, VK_QUERY_TYPE_PIPELINE_STATISTICS);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalBarrierControl);

      this->renderPassUnbindFramebuffer();

      if (suspend)
        m_flags.set(DxvkContextFlag::GpRenderPassSuspended);

      // For unsynchronized passes, we have full barrier tracking info for all
      // draws that were performed. Otherwise, emit a barrier to prevent hazards
      // with resources that were not tracked due to perf reasons.
      if (!unsynchronized) {
        if (m_renderPassBarrierSrc.stages) {
          accessMemory(DxvkCmdBuffer::ExecBuffer,
            m_renderPassBarrierSrc.stages, m_renderPassBarrierSrc.access,
            m_renderPassBarrierDst.stages, m_renderPassBarrierDst.access);

          m_renderPassBarrierSrc = DxvkGlobalPipelineBarrier();
        }

        flushBarriers();
      }

      flushResolves();

      if (!suspend)
        prepareShaderReadableImages(false);

      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        popDebugRegion(util::DxvkDebugLabelType::InternalRenderPass);
    } else if (!suspend) {
      // We may be ending a previously suspended render pass
      m_flags.clr(DxvkContextFlag::GpRenderPassSuspended);

      // Ensure that all shader readable images are ready
      // to be read and in their default layout
      prepareShaderReadableImages(false);
    }
  }


  void DxvkContext::endCurrentPass(bool suspend) {
    endRenderPass(suspend);
    endComputePass();
  }


  void DxvkContext::acquireRenderTargets(
    const DxvkFramebufferInfo&  framebufferInfo,
          DxvkRenderPassOps&    ops) {
    m_rtAccess.clear();

    // Try to perform the layout transitions on the init command buffer to avoid
    // unnecessary synchronization, especially around unsynchronized render passes
    // with a clear. We can't use the regular prepareOutOfOrderTransfer function
    // here because that will ignore bound render targets.
    DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer::InitBuffer;

    // Transition all images to the render layout as necessary
    const auto& depthAttachment = framebufferInfo.getDepthTarget();

    if (depthAttachment.view) {
      VkImageLayout layout = depthAttachment.view->getLayout();

      VkPipelineStageFlags2 depthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      VkAccessFlags2 depthAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

      if (layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        depthAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        depthStages |= m_device->getShaderPipelineStages();
        depthAccess |= VK_ACCESS_2_SHADER_READ_BIT;
      }

      VkImageLayout currentLayout = depthAttachment.view->image()->queryLayout(depthAttachment.view->imageSubresources());

      if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        if (ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_LOAD)
          ops.depthOps.loadOpD = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        if (ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_LOAD)
          ops.depthOps.loadOpS = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      }

      VkImageAspectFlags preserveAspects = depthAttachment.view->info().aspects;

      if (ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_CLEAR
       || ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        preserveAspects &= ~VK_IMAGE_ASPECT_DEPTH_BIT;

      if (ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_CLEAR
       || ops.depthOps.loadOpS == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        preserveAspects &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

      m_rtAccess.emplace_back(*depthAttachment.view, depthStages, depthAccess, !preserveAspects);

      if (!prepareOutOfOrderTransition(*depthAttachment.view->image()))
        cmdBuffer = DxvkCmdBuffer::ExecBuffer;
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorAttachment = framebufferInfo.getColorTarget(i);

      if (colorAttachment.view) {
        VkAccessFlags2 colorAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                                   | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

        VkPipelineStageFlags2 colorStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkImageLayout currentLayout = colorAttachment.view->image()->queryLayout(colorAttachment.view->imageSubresources());

        if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
          ops.colorOps[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        bool discard = ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
                    || ops.colorOps[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        m_rtAccess.emplace_back(*colorAttachment.view, colorStages, colorAccess, discard);

        if (!prepareOutOfOrderTransition(*colorAttachment.view->image()))
          cmdBuffer = DxvkCmdBuffer::ExecBuffer;
      }
    }

    // For regularly synchronized render passes, emit barriers here. We need to do this
    // even if there are no layout transitions, since we don't track resource usage during
    // render passes.
    if (!m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized))
      flushBarriers();

    // Ignore clears, we should already have processed all of them.
    acquireResources(cmdBuffer, m_rtAccess.size(), m_rtAccess.data(), false);
  }


  void DxvkContext::releaseRenderTargets() {
    releaseResources(DxvkCmdBuffer::ExecBuffer, m_rtAccess.size(), m_rtAccess.data());

    m_rtAccess.clear();
  }


  bool DxvkContext::renderPassStartUnsynchronized() {
    // TODO re-enable once we iron out the bugs.
    return false;

    // Don't even try if there is a depth buffer bound, this is most likely
    // either a shadow pass or an otherwise expensive main render pass.
    //
    // This will create some false positives, but that is okay.
    if (m_state.om.framebufferInfo.getDepthTarget().view)
      return false;

    // Same for MSAA. This is actually necessary for correctness as well
    // since handling render pass resolves gets all sorts of problematic,
    // and those cases are currently not handled.
    if (m_state.gp.state.ms.sampleCount() != VK_SAMPLE_COUNT_1_BIT)
      return false;

    // Otherwise, we are probably good. Post-processing and UI passes don't
    // typically have a lot of draws, or a lot of resources per draw.
    return true;
  }


  void DxvkContext::renderPassBindFramebuffer(
    const DxvkFramebufferInfo&  framebufferInfo,
          DxvkRenderPassOps&    ops) {
    const DxvkFramebufferSize fbSize = framebufferInfo.size();

    acquireRenderTargets(framebufferInfo, ops);

    m_state.om.attachmentMask.clear();

    VkCommandBufferInheritanceRenderingInfo renderingInheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO };
    VkCommandBufferInheritanceInfo inheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, &renderingInheritance };

    uint32_t colorInfoCount = 0;
    uint32_t lateClearCount = 0;

    std::array<VkFormat, MaxNumRenderTargets> colorFormats = { };
    std::array<VkClearAttachment, MaxNumRenderTargets> lateClears = { };

    bool hasMipmappedRt = false;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const auto& colorTarget = framebufferInfo.getColorTarget(i);

      auto& colorInfo = m_state.om.renderingInfo.color[i];
      colorInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

      if (colorTarget.view != nullptr) {
        colorFormats[i] = colorTarget.view->info().format;

        colorInfo.imageView = colorTarget.view->handle();
        colorInfo.imageLayout = colorTarget.view->getLayout();
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

        hasMipmappedRt |= colorTarget.view->image()->info().mipLevels > 1u;

        if ((colorTarget.view->image()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
         && m_device->features().khrUnifiedImageLayouts.unifiedImageLayouts) {
          auto& feedbackLoopInfo = m_state.om.renderingInfo.colorFeedbackLoop[i];
          feedbackLoopInfo = { VK_STRUCTURE_TYPE_ATTACHMENT_FEEDBACK_LOOP_INFO_EXT };
          feedbackLoopInfo.pNext = std::exchange(colorInfo.pNext, &feedbackLoopInfo);
          feedbackLoopInfo.feedbackLoopEnable = true;
        }

        // Attachment flags are used to control sRGB resolves
        if (m_device->features().khrMaintenance10.maintenance10) {
          auto& flagInfo = m_state.om.renderingInfo.colorAttachmentFlags[i];
          flagInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_FLAGS_INFO_KHR };
          flagInfo.pNext = std::exchange(colorInfo.pNext, &flagInfo);
        }
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
      depthStencilWritable = vk::getWritableAspectsForLayout(depthTarget.view->info().layout);

      if (!m_device->properties().khrMaintenance7.separateDepthStencilAttachmentAccess && depthStencilWritable)
        depthStencilWritable = depthStencilAspects;

      depthInfo.imageView = depthTarget.view->handle();
      depthInfo.imageLayout = depthTarget.view->getLayout();
      depthInfo.loadOp = ops.depthOps.loadOpD;
      depthInfo.storeOp = (depthStencilWritable & VK_IMAGE_ASPECT_DEPTH_BIT)
        ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_NONE;

      if (ops.depthOps.loadOpD == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        depthInfo.clearValue.depthStencil.depth = ops.depthOps.clearValue.depth;
        depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      }

      renderingInheritance.rasterizationSamples = depthTarget.view->image()->info().sampleCount;

      if ((depthTarget.view->image()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
       && m_device->features().khrUnifiedImageLayouts.unifiedImageLayouts) {
        auto& feedbackLoopInfo = m_state.om.renderingInfo.depthStencilFeedbackLoop;
        feedbackLoopInfo = { VK_STRUCTURE_TYPE_ATTACHMENT_FEEDBACK_LOOP_INFO_EXT };
        feedbackLoopInfo.pNext = std::exchange(depthInfo.pNext, &feedbackLoopInfo);
        feedbackLoopInfo.feedbackLoopEnable = true;
      }
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

    // Reset render area tracking, will be adjusted when drawing with viewports.
    m_state.om.renderAreaLo = VkOffset2D { int32_t(fbSize.width), int32_t(fbSize.height) };
    m_state.om.renderAreaHi = VkOffset2D { 0, 0 };

    if (lateClearCount)
      std::swap(m_state.om.renderAreaLo, m_state.om.renderAreaHi);

    // On drivers that don't natively support secondary command buffers, only use
    // them to enable MSAA resolve attachments. Also ignore render passes with only
    // one color attachment here since those tend to only have a small number of
    // draws and we are almost certainly going to use the output anyway.
    bool useSecondaryCmdBuffer = !m_device->perfHints().preferPrimaryCmdBufs
      && renderingInheritance.rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

    if (m_device->perfHints().preferRenderPassOps) {
      useSecondaryCmdBuffer = renderingInheritance.rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

      if (!m_device->perfHints().preferPrimaryCmdBufs)
        useSecondaryCmdBuffer |= depthStencilAspects || colorInfoCount > 1u || !hasMipmappedRt;
    }

    if (useSecondaryCmdBuffer) {
      // Begin secondary command buffer on tiling GPUs so that subsequent
      // resolve, discard and clear commands can modify render pass ops.
      m_flags.set(DxvkContextFlag::GpRenderPassSecondaryCmd);

      renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

      m_cmd->beginSecondaryCommandBuffer(inheritance);
    } else {
      // Begin rendering right away on regular GPUs
      m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);
    }

    if (lateClearCount) {
      VkClearRect clearRect = { };
      clearRect.rect.extent.width   = fbSize.width;
      clearRect.rect.extent.height  = fbSize.height;
      clearRect.layerCount          = fbSize.layers;

      m_cmd->cmdClearAttachments(DxvkCmdBuffer::ExecBuffer, lateClearCount, lateClears.data(), 1, &clearRect);
    }

    for (uint32_t i = 0; i < framebufferInfo.numAttachments(); i++) {
      const auto& attachment = framebufferInfo.getAttachment(i);
      m_cmd->track(attachment.view->image(), DxvkAccess::Write);

      if (attachment.view->isMultisampled()) {
        VkImageSubresourceRange subresources = attachment.view->imageSubresources();

        if (subresources.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
          subresources.aspectMask = vk::getWritableAspectsForLayout(attachment.view->info().layout);

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
      m_cmd->cmdBeginRendering(DxvkCmdBuffer::ExecBuffer, &renderingInfo);
      m_cmd->cmdExecuteCommands(1, &cmdBuffer);
    }

    // End actual rendering command
    m_cmd->cmdEndRendering(DxvkCmdBuffer::ExecBuffer);

    // Emit render target barriers
    releaseRenderTargets();
  }
  
  
  void DxvkContext::resetRenderPassOps(
    const DxvkRenderTargets&    renderTargets,
          DxvkRenderPassOps&    renderPassOps) {
    renderPassOps.depthOps = DxvkDepthAttachmentOps {
      VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD };
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      renderPassOps.colorOps[i] = DxvkColorAttachmentOps { VK_ATTACHMENT_LOAD_OP_LOAD };
  }
  
  
  void DxvkContext::startTransformFeedback() {
    if (!m_flags.test(DxvkContextFlag::GpXfbActive)) {
      m_flags.set(DxvkContextFlag::GpXfbActive);

      VkBuffer     ctrBuffers[MaxNumXfbBuffers];
      VkDeviceSize ctrOffsets[MaxNumXfbBuffers];

      for (uint32_t i = 0; i < MaxNumXfbBuffers; i++) {
        m_state.xfb.activeCounters[i] = m_state.xfb.counters[i];
        auto bufferSlice = m_state.xfb.activeCounters[i].getSliceInfo();

        ctrBuffers[i] = bufferSlice.buffer;
        ctrOffsets[i] = bufferSlice.offset;

        if (bufferSlice.buffer) {
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
        auto bufferSlice = m_state.xfb.activeCounters[i].getSliceInfo();

        ctrBuffers[i] = bufferSlice.buffer;
        ctrOffsets[i] = bufferSlice.offset;

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
    m_flags.clr(DxvkContextFlag::CpHasPushData);

    m_state.cp.pipeline = nullptr;
  }
  
  
  bool DxvkContext::updateComputePipelineState() {
    if (unlikely(m_state.gp.pipeline != nullptr))
      this->unbindGraphicsPipeline();

    m_flags.clr(DxvkContextFlag::CpHasPushData);

    // Look up pipeline object based on the bound compute shader
    auto newPipeline = lookupComputePipeline(m_state.cp.shaders);
    m_state.cp.pipeline = newPipeline;

    if (unlikely(!newPipeline))
      return false;

    auto newLayout = newPipeline->getLayout()->getLayout(DxvkPipelineLayoutType::Merged);

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

    auto pushData = newLayout->getPushData();

    if (!pushData.isEmpty()) {
      m_flags.set(DxvkContextFlag::CpHasPushData,
                  DxvkContextFlag::DirtyPushData);
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      m_cmd->cmdInsertDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0dca2, newPipeline->debugName()));
    }

    if (!m_features.test(DxvkContextFeature::DescriptorHeap)) {
      // Bind global sampler set if needed and if the previous pipeline did not use it
      if (newLayout->usesSamplerHeap())
        updateSamplerSet<VK_PIPELINE_BIND_POINT_COMPUTE>(newLayout);
    }

    m_flags.clr(DxvkContextFlag::CpDirtyPipelineState);
    return true;
  }
  
  
  void DxvkContext::unbindGraphicsPipeline() {
    m_flags.set(DxvkContextFlag::GpDirtyPipeline,
                DxvkContextFlag::GpDirtyPipelineState,
                DxvkContextFlag::GpDirtyVertexBuffers,
                DxvkContextFlag::GpDirtyIndexBuffer,
                DxvkContextFlag::GpDirtyXfbBuffers,
                DxvkContextFlag::GpDirtyBlendConstants,
                DxvkContextFlag::GpDirtyStencilTest,
                DxvkContextFlag::GpDirtyStencilRef,
                DxvkContextFlag::GpDirtyMultisampleState,
                DxvkContextFlag::GpDirtyRasterizerState,
                DxvkContextFlag::GpDirtySampleLocations,
                DxvkContextFlag::GpDirtyViewport,
                DxvkContextFlag::GpDirtyDepthBias,
                DxvkContextFlag::GpDirtyDepthBounds,
                DxvkContextFlag::GpDirtyDepthClip,
                DxvkContextFlag::GpDirtyDepthTest);

    m_flags.clr(DxvkContextFlag::GpHasPushData);

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

    m_flags.clr(DxvkContextFlag::GpDirtyPipeline);
    return true;
  }
  
  
  bool DxvkContext::updateGraphicsPipelineState() {
    auto oldPipelineLayoutType = getActivePipelineLayoutType(VK_PIPELINE_BIND_POINT_GRAPHICS);

    // Check which dynamic states need to be active. States that
    // are not dynamic will be invalidated in the command buffer.
    m_flags.clr(DxvkContextFlag::GpDynamicBlendConstants,
                DxvkContextFlag::GpDynamicDepthBias,
                DxvkContextFlag::GpDynamicDepthBounds,
                DxvkContextFlag::GpDynamicDepthClip,
                DxvkContextFlag::GpDynamicDepthTest,
                DxvkContextFlag::GpDynamicStencilTest,
                DxvkContextFlag::GpDynamicMultisampleState,
                DxvkContextFlag::GpDynamicRasterizerState,
                DxvkContextFlag::GpDynamicSampleLocations,
                DxvkContextFlag::GpHasPushData,
                DxvkContextFlag::GpIndependentSets);
    
    m_flags.set(m_state.gp.state.useDynamicBlendConstants()
      ? DxvkContextFlag::GpDynamicBlendConstants
      : DxvkContextFlag::GpDirtyBlendConstants);
    
    m_flags.set((!m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasRasterizerDiscard))
      ? DxvkContextFlags(DxvkContextFlag::GpDynamicRasterizerState,
                         DxvkContextFlag::GpDynamicDepthBias)
      : DxvkContextFlags(DxvkContextFlag::GpDirtyRasterizerState,
                         DxvkContextFlag::GpDirtyDepthBias));

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
      m_flags.set(DxvkContextFlag::GpDynamicDepthBias,
                  DxvkContextFlag::GpDynamicDepthTest,
                  DxvkContextFlag::GpDynamicStencilTest,
                  DxvkContextFlag::GpIndependentSets);

      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable)
        m_flags.set(DxvkContextFlag::GpDynamicDepthClip);

      if (m_device->features().core.features.depthBounds)
        m_flags.set(DxvkContextFlag::GpDynamicDepthBounds);

      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
       && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask) {
        m_flags.set(m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasSampleRateShading)
          ? DxvkContextFlag::GpDynamicMultisampleState
          : DxvkContextFlag::GpDirtyMultisampleState);
      }

      if (m_device->canUseSampleLocations(0u))
        m_flags.set(DxvkContextFlag::GpDynamicSampleLocations);
    } else {
      if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable)
        m_flags.set(DxvkContextFlag::GpDirtyDepthClip);

      if (m_device->features().core.features.depthBounds) {
        m_flags.set(m_state.gp.state.useDynamicDepthBounds()
          ? DxvkContextFlag::GpDynamicDepthBounds
          : DxvkContextFlag::GpDirtyDepthBounds);
      }

      if (m_device->canUseSampleLocations(0u)) {
        m_flags.set(m_state.gp.state.useSampleLocations()
          ? DxvkContextFlag::GpDynamicSampleLocations
          : DxvkContextFlag::GpDirtySampleLocations);
      }

      m_flags.set(m_state.gp.state.useDynamicDepthTest()
        ? DxvkContextFlag::GpDynamicDepthTest
        : DxvkContextFlag::GpDirtyDepthTest);

      m_flags.set(m_state.gp.state.useDynamicStencilTest()
        ? DxvkContextFlags(DxvkContextFlag::GpDynamicStencilTest)
        : DxvkContextFlags(DxvkContextFlag::GpDirtyStencilTest,
                           DxvkContextFlag::GpDirtyStencilRef));

      m_flags.set(
        DxvkContextFlag::GpDirtyMultisampleState);
    }

    // If necessary, dirty descriptor sets due to layout incompatibilities
    auto newPipelineLayoutType = getActivePipelineLayoutType(VK_PIPELINE_BIND_POINT_GRAPHICS);

    if (newPipelineLayoutType != oldPipelineLayoutType)
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);

    // Also update push constant status when we know the final layout
    auto layout = m_state.gp.pipeline->getLayout()->getLayout(newPipelineLayoutType);
    auto pushData = layout->getPushData();

    if (!pushData.isEmpty()) {
      m_flags.set(DxvkContextFlag::GpHasPushData,
                  DxvkContextFlag::DirtyPushData);
    }

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

    if (!m_features.test(DxvkContextFeature::DescriptorHeap)) {
      // If the new pipeline uses the global sampler set when the
      // previous one didn't, re-bind it to the static set index 0.
      if (layout->usesSamplerHeap())
        updateSamplerSet<VK_PIPELINE_BIND_POINT_GRAPHICS>(layout);
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
  void DxvkContext::updateSamplerSet(const DxvkPipelineLayout* layout) {
    if (m_features.test(DxvkContextFeature::DescriptorBuffer)) {
      const uint32_t     bufferIndex = 0u;
      const VkDeviceSize bufferOffset = 0u;

      m_cmd->cmdSetDescriptorBufferOffsetsEXT(DxvkCmdBuffer::ExecBuffer,
        BindPoint, layout->getPipelineLayout(), 0u, 1u,
        &bufferIndex, &bufferOffset);
    } else {
      VkDescriptorSet set = m_device->getSamplerDescriptorSet().set;

      m_cmd->cmdBindDescriptorSets(DxvkCmdBuffer::ExecBuffer,
        BindPoint, layout->getPipelineLayout(), 0u, 1u, &set);
    }
  }


  template<VkPipelineBindPoint BindPoint, bool AlwaysTrack>
  bool DxvkContext::updateResourceBindings(const DxvkPipelineBindings* layout) {
    if (m_features.test(DxvkContextFeature::DescriptorHeap)) {
      if (!updateDescriptorHeapBindings<BindPoint, DxvkBindingModel::DescriptorHeap, AlwaysTrack>(layout))
        return false;
    } else if (m_features.test(DxvkContextFeature::DescriptorBuffer)) {
      if (!updateDescriptorHeapBindings<BindPoint, DxvkBindingModel::DescriptorBuffer, AlwaysTrack>(layout))
        return false;
    } else {
      updateDescriptorSetsBindings<BindPoint, AlwaysTrack>(layout);
    }

    updatePushDataBindings<BindPoint, AlwaysTrack>(layout);
    return true;
  }


  template<VkPipelineBindPoint BindPoint, bool AlwaysTrack>
  void DxvkContext::updateDescriptorSetsBindings(const DxvkPipelineBindings* layout) {
    constexpr bool TrackBindings = AlwaysTrack || BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE;

    DxvkPipelineLayoutType pipelineLayoutType = getActivePipelineLayoutType(BindPoint);
    const auto* pipelineLayout = layout->getLayout(pipelineLayoutType);

    // Ensure that the arrays we write descriptor info to are big enough
    if (unlikely(layout->getDescriptorCount() > m_legacyDescriptors.infos.size()))
      this->resizeDescriptorArrays(layout->getDescriptorCount());

    // Find out which sets we actually need to update based on the pipeline
    // layout. This may be an empty mask if only unrelated resources were
    // changed, but we have no way of knowing that up-front.
    uint32_t dirtySetMask = layout->getDirtySetMask(pipelineLayoutType, m_descriptorState);

    if (likely(dirtySetMask)) {
      // On 32-bit wine, vkUpdateDescriptorSets has significant overhead due
      // to struct conversion, so we should use descriptor update templates.
      // For 64-bit applications, using templates is slower on some drivers.
      constexpr bool useDescriptorTemplates = env::is32BitHostPlatform();

      std::array<VkDescriptorSet, DxvkDescriptorSets::SetCount> sets = { };
      m_descriptorPool->alloc(m_trackingId, pipelineLayout, dirtySetMask, sets.data());

      uint32_t descriptorCount = 0;

      for (auto setIndex : bit::BitMask(dirtySetMask)) {
        auto range = layout->getAllDescriptorsInSet(pipelineLayoutType, setIndex);

        for (uint32_t j = 0; j < range.bindingCount; j++) {
          const auto& binding = range.bindings[j];

          if (!useDescriptorTemplates) {
            auto& descriptorWrite = m_legacyDescriptors.writes[descriptorCount];
            descriptorWrite.dstSet = sets[setIndex];
            descriptorWrite.dstBinding = binding.getBinding();
            descriptorWrite.dstArrayElement = binding.getArrayIndex();
            descriptorWrite.descriptorType = binding.getDescriptorType();
          }

          auto& descriptorInfo = m_legacyDescriptors.infos[descriptorCount++];

          if (binding.isUniformBuffer()) {
            const auto& slice = m_uniformBuffers[binding.getResourceIndex()];

            if (slice.length()) {
              auto bufferInfo = slice.getSliceInfo();
              descriptorInfo.buffer.buffer = bufferInfo.buffer;
              descriptorInfo.buffer.offset = bufferInfo.offset;
              descriptorInfo.buffer.range = bufferInfo.size;

              trackUniformBufferBinding<TrackBindings>(binding, slice);
            } else {
              descriptorInfo.buffer.buffer = VK_NULL_HANDLE;
              descriptorInfo.buffer.offset = 0;
              descriptorInfo.buffer.range = VK_WHOLE_SIZE;
            }
          } else {
            switch (binding.getDescriptorType()) {
              case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.imageView)
                  descriptor = res.imageView->getDescriptor(binding.getViewType());

                if (descriptor) {
                  if (likely(!res.imageView->isMultisampled() || binding.isMultisampled())) {
                    descriptorInfo = descriptor->legacy;

                    trackImageViewBinding<TrackBindings, false>(binding, *res.imageView);
                  } else if (m_device->config().enableImplicitResolves) {
                    auto view = m_implicitResolves.getResolveView(*res.imageView, m_trackingId);
                    descriptorInfo = view->getDescriptor(binding.getViewType())->legacy;

                    m_cmd->track(view->image(), DxvkAccess::Read);
                  } else {
                    descriptorInfo.image.sampler = VK_NULL_HANDLE;
                    descriptorInfo.image.imageView = VK_NULL_HANDLE;
                    descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                  }
                } else {
                  descriptorInfo.image.sampler = VK_NULL_HANDLE;
                  descriptorInfo.image.imageView = VK_NULL_HANDLE;
                  descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                }
              } break;

              case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.imageView)
                  descriptor = res.imageView->getDescriptor(binding.getViewType());

                if (descriptor) {
                  descriptorInfo = descriptor->legacy;

                  trackImageViewBinding<TrackBindings, true>(binding, *res.imageView);
                } else {
                  descriptorInfo.image.sampler = VK_NULL_HANDLE;
                  descriptorInfo.image.imageView = VK_NULL_HANDLE;
                  descriptorInfo.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                }
              } break;

              case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.bufferView)
                  descriptor = res.bufferView->getDescriptor(false);

                if (descriptor) {
                  descriptorInfo = descriptor->legacy;

                  trackBufferViewBinding<TrackBindings, false>(binding, *res.bufferView);
                } else {
                  descriptorInfo.bufferView = VK_NULL_HANDLE;
                }
              } break;

              case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.bufferView)
                  descriptor = res.bufferView->getDescriptor(false);

                if (descriptor) {
                  descriptorInfo = descriptor->legacy;

                  trackBufferViewBinding<TrackBindings, true>(binding, *res.bufferView);
                } else {
                  descriptorInfo.bufferView = VK_NULL_HANDLE;
                }
              } break;

              case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.bufferView)
                  descriptor = res.bufferView->getDescriptor(true);

                if (descriptor) {
                  descriptorInfo = descriptor->legacy;

                  trackBufferViewBinding<TrackBindings, false>(binding, *res.bufferView);
                } else {
                  descriptorInfo.buffer.buffer = VK_NULL_HANDLE;
                  descriptorInfo.buffer.offset = 0;
                  descriptorInfo.buffer.range = VK_WHOLE_SIZE;
                }
              } break;

              case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                const auto& res = m_resources[binding.getResourceIndex()];
                const DxvkDescriptor* descriptor = nullptr;

                if (res.bufferView)
                  descriptor = res.bufferView->getDescriptor(true);

                if (descriptor) {
                  descriptorInfo = descriptor->legacy;

                  trackBufferViewBinding<TrackBindings, true>(binding, *res.bufferView);
                } else {
                  descriptorInfo.buffer.buffer = VK_NULL_HANDLE;
                  descriptorInfo.buffer.offset = 0;
                  descriptorInfo.buffer.range = VK_WHOLE_SIZE;
                }
              } break;

              default:
                /* nothing to do */;
            }
          }
        }

        if (useDescriptorTemplates) {
          m_cmd->updateDescriptorSetWithTemplate(sets[setIndex],
            pipelineLayout->getDescriptorSetLayout(setIndex)->getSetUpdateTemplate(),
            m_legacyDescriptors.infos.data());
          descriptorCount = 0;
        }
      }

      // Update all descriptors in one go to avoid API call overhead
      if (!useDescriptorTemplates) {
        m_cmd->updateDescriptorSets(descriptorCount,
          m_legacyDescriptors.writes.data());
      }

      do {
        // Similarly, bind consecutive descriptor set ranges at once.
        uint32_t first = bit::bsf(dirtySetMask);

        // Add the lowest set bit to the mask and count the number of
        // additional zeroes we created to get the final set count
        uint32_t countMask = dirtySetMask + (dirtySetMask & -dirtySetMask);
        uint32_t count = bit::bsf(countMask) - first;

        // Global sampler set will always be bound to index 0
        uint32_t setIndex = first + uint32_t(pipelineLayout->usesSamplerHeap());

        m_cmd->cmdBindDescriptorSets(DxvkCmdBuffer::ExecBuffer,
          BindPoint, pipelineLayout->getPipelineLayout(),
          setIndex, count, &sets[first]);

        dirtySetMask &= countMask;
      } while (dirtySetMask);
    }
  }


  template<VkPipelineBindPoint BindPoint, DxvkBindingModel Model, bool AlwaysTrack>
  bool DxvkContext::updateDescriptorHeapBindings(const DxvkPipelineBindings* layout) {
    constexpr bool TrackBindings = AlwaysTrack || BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE;
    using HeapOffset = std::conditional_t<Model == DxvkBindingModel::DescriptorHeap, uint32_t, VkDeviceSize>;

    DxvkPipelineLayoutType pipelineLayoutType = getActivePipelineLayoutType(BindPoint);
    const auto* pipelineLayout = layout->getLayout(pipelineLayoutType);

    // Check if there's anything to do; the mask can be empty
    // in case only unrelated bindings have been updated.
    uint32_t dirtySetMask = layout->getDirtySetMask(pipelineLayoutType, m_descriptorState);

    if (unlikely(!dirtySetMask))
      return true;

    // Make sure we have enough space for the set. If this fails, the caller
    // has to make sure that no secondary command buffer is active. If we
    // allocate a new descriptor range and potentially re-bind the heap, we
    // need to re-allocate all sets too.
    if (!m_cmd->canAllocateDescriptors(pipelineLayout)) {
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT);

      if (!m_cmd->createDescriptorRange())
        return false;

      if (Model != DxvkBindingModel::DescriptorHeap) {
        if (pipelineLayout->usesSamplerHeap())
          updateSamplerSet<BindPoint>(pipelineLayout);
      }

      dirtySetMask = layout->getDirtySetMask(pipelineLayoutType, m_descriptorState);
    }

    std::array<uint32_t, DxvkDescriptorSets::SetCount> bufferIndices = { };
    std::array<HeapOffset, DxvkDescriptorSets::SetCount> heapOffsets = { };

    if constexpr (Model == DxvkBindingModel::DescriptorHeap) {
      // Make sure the heaps are actually valid and usable
      m_cmd->ensureDescriptorHeapBinding();
    } else {
      // The resource heap is always bound at index 1
      for (auto& index : bufferIndices)
        index = 1u;
    }

    // Scratch memory for descriptor updates
    for (auto setIndex : bit::BitMask(dirtySetMask)) {
      auto range = layout->getAllDescriptorsInSet(pipelineLayoutType, setIndex);

      auto setLayout = pipelineLayout->getDescriptorSetLayout(setIndex);

      // Allocate descriptor set in memory and query heap offset
      auto setStorage = m_cmd->allocateDescriptors(setLayout);
      heapOffsets[setIndex] = setStorage.offset >> pipelineLayout->getDescriptorOffsetShift();

      // Allocate descriptor update entry to write descriptor pointers to
      auto e = m_descriptorWorker.allocEntry(setLayout, setStorage.mapPtr, range.bindingCount,
        layout->getUniformBuffersInSet(pipelineLayoutType, setIndex).bindingCount);

      size_t bufferCount = 0u;

      for (uint32_t j = 0; j < range.bindingCount; j++) {
        const auto& binding = range.bindings[j];

        if (binding.isUniformBuffer()) {
          const auto& slice = m_uniformBuffers[binding.getResourceIndex()];
          auto sliceInfo = slice.getSliceInfo();

          auto& buffer = e.buffers[bufferCount++];
          buffer.gpuAddress = sliceInfo.gpuAddress;
          buffer.size = sliceInfo.size;
          buffer.indexInSet = j;
          buffer.descriptorType = uint16_t(binding.getDescriptorType());

          if (likely(sliceInfo.size))
            trackUniformBufferBinding<TrackBindings>(binding, slice);
        } else {
          const auto& res = m_resources[binding.getResourceIndex()];

          switch (binding.getDescriptorType()) {
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
              if (res.imageView && likely(e.descriptors[j] = res.imageView->getDescriptor(binding.getViewType()))) {
                if (likely(!res.imageView->isMultisampled() || binding.isMultisampled())) {
                  trackImageViewBinding<TrackBindings, false>(binding, *res.imageView);
                  break;
                } else if (m_device->config().enableImplicitResolves) {
                  auto view = m_implicitResolves.getResolveView(*res.imageView, m_trackingId);

                  if (likely(e.descriptors[j] = view->getDescriptor(binding.getViewType()))) {
                    m_cmd->track(view->image(), DxvkAccess::Read);
                    break;
                  }
                }
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            } break;

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
              if (res.imageView && likely(e.descriptors[j] = res.imageView->getDescriptor(binding.getViewType()))) {
                trackImageViewBinding<TrackBindings, true>(binding, *res.imageView);
                break;
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            } break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
              if (res.bufferView && likely(e.descriptors[j] = res.bufferView->getDescriptor(false))) {
                trackBufferViewBinding<TrackBindings, false>(binding, *res.bufferView);
                break;
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
            } break;

            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
              if (res.bufferView && likely(e.descriptors[j] = res.bufferView->getDescriptor(false))) {
                trackBufferViewBinding<TrackBindings, true>(binding, *res.bufferView);
                break;
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
            } break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
              if (res.bufferView && likely(e.descriptors[j] = res.bufferView->getDescriptor(true))) {
                trackBufferViewBinding<TrackBindings, false>(binding, *res.bufferView);
                break;
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            } break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
              if (res.bufferView && likely(e.descriptors[j] = res.bufferView->getDescriptor(true))) {
                trackBufferViewBinding<TrackBindings, true>(binding, *res.bufferView);
                break;
              }

              e.descriptors[j] = m_device->getDescriptorProperties().getNullDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            } break;

            default:
              /* Nothing to do */;
          }
        }
      }
    }

    do {
      // Bind consecutive descriptor ranges at once
      uint32_t first = bit::bsf(dirtySetMask);

      // Add the lowest set bit to the mask and count the number of
      // additional zeroes we created to get the final set count
      uint32_t countMask = dirtySetMask + (dirtySetMask & -dirtySetMask);
      uint32_t count = bit::bsf(countMask) - first;

      if constexpr (Model == DxvkBindingModel::DescriptorHeap) {
        // Descriptor set offsets are stored in-order at offset 0
        VkPushDataInfoEXT pushInfo = { VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT };
        pushInfo.offset = sizeof(uint32_t) * first;
        pushInfo.data.address = &heapOffsets[first];
        pushInfo.data.size = sizeof(uint32_t) * count;

        m_cmd->cmdPushData(DxvkCmdBuffer::ExecBuffer, &pushInfo);
      } else {
        // Global sampler set will always be bound to index 0 if used
        uint32_t setIndex = first + uint32_t(pipelineLayout->usesSamplerHeap());

        m_cmd->cmdSetDescriptorBufferOffsetsEXT(DxvkCmdBuffer::ExecBuffer,
          BindPoint, pipelineLayout->getPipelineLayout(), setIndex, count,
          &bufferIndices[first], &heapOffsets[first]);
      }

      dirtySetMask &= countMask;
    } while (dirtySetMask);

    return true;
  }


  template<VkPipelineBindPoint BindPoint, bool AlwaysTrack>
  void DxvkContext::updatePushDataBindings(const DxvkPipelineBindings* layout) {
    constexpr bool TrackBindings = AlwaysTrack || BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE;

    DxvkPipelineLayoutType pipelineLayoutType = getActivePipelineLayoutType(BindPoint);

    if (m_descriptorState.hasDirtyVas(layout->getNonemptyStageMask())) {
      auto range = layout->getVaBindings(pipelineLayoutType);

      if (range.bindingCount)
        m_flags.set(DxvkContextFlag::DirtyPushData);

      for (uint32_t i = 0u; i < range.bindingCount; i++) {
        const auto& binding = range.bindings[i];
        const auto& res = m_resources[binding.getResourceIndex()];

        VkDeviceAddress va = 0u;

        if (binding.isUniformBuffer()) {
          const auto& slice = m_uniformBuffers[binding.getResourceIndex()];

          if (slice.length()) {
            va = slice.getSliceInfo().gpuAddress;

            trackUniformBufferBinding<TrackBindings>(binding, slice);
          }
        } else {
          if (res.bufferView) {
            va = res.bufferView->getSliceInfo().gpuAddress;

            trackBufferViewBinding<TrackBindings, true>(binding, *res.bufferView);
          }
        }

        std::memcpy(&m_state.pc.resourceData[binding.getBlockOffset()], &va, sizeof(va));
      }
    }

    if (m_descriptorState.hasDirtySamplers(layout->getNonemptyStageMask())) {
      auto range = layout->getSamplers(pipelineLayoutType);

      if (range.bindingCount)
        m_flags.set(DxvkContextFlag::DirtyPushData);

      for (uint32_t i = 0u; i < range.bindingCount; i++) {
        const auto& binding = range.bindings[i];
        const auto& sampler = m_samplers[binding.getResourceIndex()];

        uint16_t index = 0u;

        if (likely(sampler)) {
          index = sampler->getDescriptor().samplerIndex;
          m_cmd->track(sampler);
        }

        std::memcpy(&m_state.pc.resourceData[binding.getBlockOffset()], &index, sizeof(index));
      }
    }
  }


  void DxvkContext::updateComputeShaderResources() {
    this->updateResourceBindings<VK_PIPELINE_BIND_POINT_COMPUTE, true>(
      m_state.cp.pipeline->getLayout());

    m_descriptorState.clearStages(VK_SHADER_STAGE_COMPUTE_BIT);
  }
  
  
  bool DxvkContext::updateGraphicsShaderResources() {
    bool status;

    if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)) {
      // Enable full resource tracking inside unsynchronized render passes. This
      // does come at a hefty CPU performance cost, so at least optimize the code
      // in such a way that we skip all the checks for GFX stores.
      status = updateResourceBindings<VK_PIPELINE_BIND_POINT_GRAPHICS, true>(m_state.gp.pipeline->getLayout());
    } else {
      // In regularly synchronized passes, only track resources with GFX stores.
      status = updateResourceBindings<VK_PIPELINE_BIND_POINT_GRAPHICS, false>(m_state.gp.pipeline->getLayout());
    }

    if (!status)
      return false;

    m_descriptorState.clearStages(VK_SHADER_STAGE_ALL_GRAPHICS);
    return true;
  }
  
  
  DxvkFramebufferInfo DxvkContext::makeFramebufferInfo(
    const DxvkRenderTargets&      renderTargets) {
    return DxvkFramebufferInfo(renderTargets, m_device->getDefaultFramebufferSize());
  }


  void DxvkContext::updateRenderTargets() {
    if (m_flags.test(DxvkContextFlag::GpDirtyRenderTargets)) {
      m_flags.clr(DxvkContextFlag::GpDirtyRenderTargets);

      if (m_flags.test(DxvkContextFlag::GpRenderPassActive) && !m_flags.test(DxvkContextFlag::GpRenderPassNeedsFlush)) {
        // Only interrupt an active render pass if the render targets have actually
        // changed since the last update. There are cases where client APIs cannot
        // know in advance that consecutive draws use the same set of render targets.
        if (m_state.om.renderTargets == m_state.om.framebufferInfo.attachments())
          return;
      }

      // End active render pass and reset load/store ops for the new render targets.
      DxvkFramebufferInfo fbInfo = makeFramebufferInfo(m_state.om.renderTargets);

      this->endCurrentPass(true);

      this->resetRenderPassOps(
        m_state.om.renderTargets,
        m_state.om.renderPassOps);

      // Update relevant graphics pipeline state
      m_state.gp.state.ms.setSampleCount(fbInfo.getSampleCount());
      m_state.gp.state.rt = fbInfo.getRtInfo();

      for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
        const auto& attachment = fbInfo.getColorTarget(i).view;

        VkComponentMapping mapping = VkComponentMapping();

        if (attachment)
          mapping = util::invertComponentMapping(attachment->info().unpackSwizzle());

        m_state.gp.state.omSwizzle[i] = DxvkOmAttachmentSwizzle(mapping);
      }

      m_state.om.framebufferInfo = std::move(fbInfo);

      m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
    } else if (m_flags.test(DxvkContextFlag::GpRenderPassNeedsFlush)) {
      // End render pass to flush pending resolves
      this->endCurrentPass(true);
    }
  }


  bool DxvkContext::flushDeferredClear(
    const DxvkImage&              image,
    const VkImageSubresourceRange& subresources) {
    if (!(image.info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)))
      return false;

    DxvkClearBatch clearBatch;

    size_t srcIndex = 0u;
    size_t dstIndex = 0u;

    while (srcIndex < m_deferredClears.size()) {
      auto& entry = m_deferredClears[srcIndex];

      if ((entry.imageView->image() == &image)
       && ((entry.clearAspects | entry.discardAspects) | subresources.aspectMask)
       && (vk::checkSubresourceRangeOverlap(entry.imageView->imageSubresources(), subresources))) {
        clearBatch.add(batchClear(entry.imageView, -1, entry.discardAspects, entry.clearAspects, entry.clearValue));
        srcIndex += 1u;
       } else {
        if (dstIndex < srcIndex)
          m_deferredClears[dstIndex] = std::move(m_deferredClears[srcIndex]);

        dstIndex += 1u;
        srcIndex += 1u;
      }
    }

    if (dstIndex < srcIndex) {
      m_deferredClears.resize(dstIndex);

      // Need to call this *after* removing the clear from the
      // list since this will try to run clears out-of-order.
      performClears(clearBatch);
      return true;
    } else {
      return false;
    }
  }


  DxvkDeferredClear* DxvkContext::findDeferredClear(
    const DxvkImage&              image,
    const VkImageSubresourceRange& subresources) {
    for (auto& entry : m_deferredClears) {
      if ((entry.imageView->image() == &image) && ((subresources.aspectMask & entry.clearAspects) == subresources.aspectMask)
       && (vk::checkSubresourceRangeSuperset(entry.imageView->imageSubresources(), subresources)))
        return &entry;
    }

    return nullptr;
  }


  DxvkDeferredClear* DxvkContext::findOverlappingDeferredClear(
    const DxvkImage&              image,
    const VkImageSubresourceRange& subresources) {
    for (auto& entry : m_deferredClears) {
      if ((entry.imageView->image() == &image)
       && ((entry.clearAspects | entry.discardAspects) | subresources.aspectMask)
       && (vk::checkSubresourceRangeOverlap(entry.imageView->imageSubresources(), subresources)))
        return &entry;
    }

    return nullptr;
  }


  DxvkDeferredResolve* DxvkContext::findOverlappingDeferredResolve(
    const DxvkImage&              image,
    const VkImageSubresourceRange& subresources) {
    for (auto& entry : m_deferredResolves) {
      if (entry.imageView && entry.imageView->image() == &image
       && (vk::checkSubresourceRangeOverlap(entry.imageView->imageSubresources(), subresources)))
        return &entry;
    }

    return nullptr;
  }


  bool DxvkContext::isBoundAsRenderTarget(
    const DxvkImage&              image,
    const VkImageSubresourceRange& subresources) {
    for (uint32_t i = 0; i < m_state.om.framebufferInfo.numAttachments(); i++) {
      auto& view = m_state.om.framebufferInfo.getAttachment(i).view;

      if (view && view->image() == &image && vk::checkSubresourceRangeOverlap(view->imageSubresources(), subresources))
        return true;
    }

    return false;
  }


  void DxvkContext::updateIndexBufferBinding() {
    m_flags.clr(DxvkContextFlag::GpDirtyIndexBuffer);

    if (likely(m_state.vi.indexBuffer.length())) {
      auto bufferInfo = m_state.vi.indexBuffer.getSliceInfo();

      VkDeviceSize align = m_state.vi.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
      VkDeviceSize length = bufferInfo.size & ~(align - 1);

      m_cmd->cmdBindIndexBuffer2(
        bufferInfo.buffer, bufferInfo.offset,
        length, m_state.vi.indexType);

      if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
       || m_state.vi.indexBuffer.buffer()->hasGfxStores()) {
        accessBuffer(DxvkCmdBuffer::ExecBuffer, m_state.vi.indexBuffer,
          VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, DxvkAccessOp::None);
      }

      m_renderPassBarrierSrc.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      m_renderPassBarrierSrc.access |= VK_ACCESS_INDEX_READ_BIT;

      m_cmd->track(m_state.vi.indexBuffer.buffer(), DxvkAccess::Read);
    } else {
      // Bind null index buffer to read all zeroes, not too useful but well-defined
      m_cmd->cmdBindIndexBuffer2(VK_NULL_HANDLE, 0, VK_WHOLE_SIZE, m_state.vi.indexType);
    }
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
        auto vbo = m_state.vi.vertexBuffers[binding].getSliceInfo();
        
        buffers[i] = vbo.buffer;
        offsets[i] = vbo.offset;
        lengths[i] = vbo.size;
        strides[i] = m_state.vi.vertexStrides[binding];

        if (strides[i]) {
          // Dynamic strides are only allowed if the stride is not smaller
          // than highest attribute offset + format size for given binding
          newDynamicStrides &= strides[i] >= m_state.vi.vertexExtents[i];
        }

        if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)
         || m_state.vi.vertexBuffers[binding].buffer()->hasGfxStores()) {
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
    const auto& gsInfo = m_state.gp.shaders.gs->metadata();

    VkBuffer     xfbBuffers[MaxNumXfbBuffers];
    VkDeviceSize xfbOffsets[MaxNumXfbBuffers];
    VkDeviceSize xfbLengths[MaxNumXfbBuffers];

    for (size_t i = 0; i < MaxNumXfbBuffers; i++) {
      auto bufferSlice = m_state.xfb.buffers[i].getSliceInfo();
      
      xfbBuffers[i] = bufferSlice.buffer;
      xfbOffsets[i] = bufferSlice.offset;
      xfbLengths[i] = bufferSlice.size;

      if (!bufferSlice.buffer)
        xfbBuffers[i] = m_common->dummyResources().bufferInfo().buffer;

      if (bufferSlice.buffer) {
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
        const auto& viewport = m_state.vp.viewports[i];
        const auto& scissor = m_state.vp.scissorRects[i];

        // Need to floor scissor to viewport region to match D3D rules
        VkOffset2D lo = {
          std::max(scissor.offset.x, int32_t(std::max(0.0f, viewport.x))),
          std::max(scissor.offset.y, int32_t(std::max(0.0f, std::min(viewport.y, viewport.y + viewport.height)))) };

        VkOffset2D hi = {
          std::min(int32_t(renderSize.width),  int32_t(std::max(0.0f, viewport.x + viewport.width))),
          std::min(int32_t(renderSize.height), int32_t(std::max(0.0f, std::max(viewport.y, viewport.y + viewport.height)))) };

        hi.x = std::max(hi.x, lo.x);
        hi.y = std::max(hi.y, lo.y);

        auto& dst = clampedScissors[i];
        dst.offset = lo;
        dst.extent = VkExtent2D {
          std::min(scissor.extent.width,  uint32_t(hi.x - lo.x)),
          std::min(scissor.extent.height, uint32_t(hi.y - lo.y)) };

        // Extend render area based on the final scissor rect
        m_state.om.renderAreaLo = {
          std::min(m_state.om.renderAreaLo.x, dst.offset.x),
          std::min(m_state.om.renderAreaLo.y, dst.offset.y) };
        m_state.om.renderAreaHi = {
          std::max(m_state.om.renderAreaHi.x, int32_t(dst.offset.x + dst.extent.width)),
          std::max(m_state.om.renderAreaHi.y, int32_t(dst.offset.y + dst.extent.height)) };
      }

      m_cmd->cmdSetViewport(m_state.vp.viewportCount, m_state.vp.viewports.data());
      m_cmd->cmdSetScissor(m_state.vp.viewportCount, clampedScissors.data());
    }

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtyDepthClip,
                             DxvkContextFlag::GpDynamicDepthClip))) {
      m_flags.clr(DxvkContextFlag::GpDirtyDepthClip);

      m_cmd->cmdSetDepthClipState(m_state.gp.state.rs.depthClipEnable());
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

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtySampleLocations,
                             DxvkContextFlag::GpDynamicSampleLocations))) {
      m_flags.clr(DxvkContextFlag::GpDirtySampleLocations);

      // While technically undefined behaviour according to the Vulkan spec, we do not track
      // whether an image has been rendered to using centered or default sample locations.
      // On AMD hardware, it seems like samples may be reordered depending on their position,
      // and the interpretation of depth-stencil image contents can change depending on the
      // sample locations used for rendering said content. We can generally expect games to
      // render most of its content with regular sample positions and only draw a small portion
      // with centered positions, so we would want the default interpretation to use default
      // sample positions anyway, e.g. for the purpose of copies or resolves.
      VkSampleCountFlagBits msSampleCount = VkSampleCountFlagBits(m_state.gp.state.ms.sampleCount());
      VkSampleCountFlagBits rsSampleCount = VkSampleCountFlagBits(m_state.gp.state.rs.sampleCount());

      if (!msSampleCount)
        msSampleCount = rsSampleCount ? rsSampleCount : VK_SAMPLE_COUNT_1_BIT;

      bool center = m_state.gp.state.useSampleLocations();
      bool enable = m_device->canUseSampleLocations(msSampleCount);

      if (enable && m_state.om.renderTargets.depth.view) {
        auto flags = m_state.om.renderTargets.depth.view->image()->info().flags;
        enable = bool(flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT);
      }

      VkSampleLocationsInfoEXT locations = util::setupSampleLocations(msSampleCount, center);
      m_cmd->cmdSetSampleLocations(enable && center, &locations);
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

    if (unlikely(m_flags.all(DxvkContextFlag::GpDirtyDepthTest,
                             DxvkContextFlag::GpDynamicDepthTest))) {
      m_flags.clr(DxvkContextFlag::GpDirtyDepthTest);

      auto dsReadOnlyAspects = m_state.gp.state.rt.getDepthStencilReadOnlyAspects();
      bool writable = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_DEPTH_BIT);

      if (m_state.dyn.depthStencilState.depthTest()) {
        bool written = writable &&
          m_state.dyn.depthStencilState.depthTest() &&
          m_state.dyn.depthStencilState.depthWrite();

        m_cmd->cmdSetDepthTest(VK_TRUE);
        m_cmd->cmdSetDepthWrite(written);
        m_cmd->cmdSetDepthCompareOp(m_state.dyn.depthStencilState.depthCompareOp());

        if (written)
          m_state.om.attachmentMask.trackDepthWrite();

        m_state.om.attachmentMask.trackDepthRead();
      } else {
        m_cmd->cmdSetDepthTest(VK_FALSE);
        m_cmd->cmdSetDepthWrite(VK_FALSE);
        m_cmd->cmdSetDepthCompareOp(VK_COMPARE_OP_ALWAYS);
      }
    }

    if (m_flags.all(DxvkContextFlag::GpDirtyStencilTest,
                    DxvkContextFlag::GpDynamicStencilTest)) {
      m_flags.clr(DxvkContextFlag::GpDirtyStencilTest);

      if (m_state.dyn.depthStencilState.stencilTest()) {
        auto dsReadOnlyAspects = m_state.gp.state.rt.getDepthStencilReadOnlyAspects();
        bool writable = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

        auto front = convertStencilOp(m_state.dyn.depthStencilState.stencilOpFront(), writable);
        auto back = convertStencilOp(m_state.dyn.depthStencilState.stencilOpBack(), writable);

        m_cmd->cmdSetStencilTest(VK_TRUE);

        m_cmd->cmdSetStencilOp(VK_STENCIL_FACE_FRONT_BIT, front);
        m_cmd->cmdSetStencilOp(VK_STENCIL_FACE_BACK_BIT, back);

        m_cmd->cmdSetStencilCompareMask(VK_STENCIL_FACE_FRONT_BIT, front.compareMask);
        m_cmd->cmdSetStencilCompareMask(VK_STENCIL_FACE_BACK_BIT, back.compareMask);

        m_cmd->cmdSetStencilWriteMask(VK_STENCIL_FACE_FRONT_BIT, front.writeMask);
        m_cmd->cmdSetStencilWriteMask(VK_STENCIL_FACE_BACK_BIT, back.writeMask);

        if (front.writeMask | back.writeMask)
          m_state.om.attachmentMask.trackStencilWrite();

        m_state.om.attachmentMask.trackStencilRead();
      } else {
        VkStencilOpState state = { };
        state.compareOp = VK_COMPARE_OP_ALWAYS;

        m_cmd->cmdSetStencilTest(VK_FALSE);
        m_cmd->cmdSetStencilOp(VK_STENCIL_FACE_FRONT_AND_BACK, state);
        m_cmd->cmdSetStencilCompareMask(VK_STENCIL_FACE_FRONT_AND_BACK, 0u);
        m_cmd->cmdSetStencilWriteMask(VK_STENCIL_FACE_FRONT_AND_BACK, 0u);
      }
    }

    if (m_flags.all(DxvkContextFlag::GpDirtyStencilRef,
                    DxvkContextFlag::GpDynamicStencilTest)) {
      m_flags.clr(DxvkContextFlag::GpDirtyStencilRef);

      m_cmd->cmdSetStencilReference(VK_STENCIL_FRONT_AND_BACK,
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

      if (m_state.dyn.depthBounds.minDepthBounds > 0.0f
       || m_state.dyn.depthBounds.maxDepthBounds < 1.0f)
        m_state.om.attachmentMask.trackDepthRead();
    }
  }


  template<VkPipelineBindPoint BindPoint>
  void DxvkContext::updatePushData() {
    m_flags.clr(DxvkContextFlag::DirtyPushData);

    // Optimized pipelines may have push constants trimmed, so look
    // up the exact layout used for the currently bound pipeline.
    auto layoutType = getActivePipelineLayoutType(BindPoint);

    auto layout = BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
      ? m_state.gp.pipeline->getLayout()->getLayout(layoutType)
      : m_state.cp.pipeline->getLayout()->getLayout(layoutType);

    auto pushData = layout->getPushData();

    if (unlikely(pushData.isEmpty()))
      return;

    // If all push data comes from resource updates, it is already in
    // the correct layout, otherwise gather data into a temporary array.
    std::array<char, MaxTotalPushDataSize> localData;

    VkPushDataInfoEXT pushInfo = { VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT };
    pushInfo.offset = pushData.getOffset();
    pushInfo.data.address = &m_state.pc.resourceData[pushData.getOffset()];
    pushInfo.data.size = pushData.getSize();

    if ((bit::tzcnt(pushData.getResourceDwordMask() + 1u) * 4u) < pushData.getSize()) {
      pushInfo.data.address = &localData[pushData.getOffset()];

      for (auto i : bit::BitMask(layout->getPushDataMask())) {
        auto block = layout->getPushDataBlock(i);
        auto blockSize = block.getSize();

        auto srcOffset = computePushDataBlockOffset(i);
        auto dstOffset = block.getOffset();

        auto constantData = &m_state.pc.constantData[srcOffset];
        auto resourceData = &m_state.pc.resourceData[dstOffset];

        auto dstData = &localData[dstOffset];

        uint32_t rangeOffset = 0u;

        // Copy chunks of dwords either from the constant data array or
        // the resource data array, depending on the resource mask.
        uint64_t resourceMask = block.getResourceDwordMask();

        while (resourceMask) {
          uint32_t dwordIndex = bit::tzcnt(resourceMask);
          uint32_t dwordCount = bit::tzcnt(resourceMask + (resourceMask & -resourceMask));

          uint32_t byteIndex = dwordIndex * sizeof(uint32_t);
          uint32_t byteCount = dwordCount * sizeof(uint32_t);

          std::memcpy(&dstData[rangeOffset],
            &constantData[rangeOffset], byteIndex);

          std::memcpy(&dstData[rangeOffset + byteIndex],
            &resourceData[rangeOffset + byteIndex],
            byteCount - byteIndex);

          resourceMask >>= dwordCount;
          rangeOffset += byteCount;
        }

        std::memcpy(&dstData[rangeOffset],
          &constantData[rangeOffset], blockSize - rangeOffset);
      }
    }

    if (m_features.test(DxvkContextFeature::DescriptorHeap)) {
      m_cmd->cmdPushData(DxvkCmdBuffer::ExecBuffer, &pushInfo);
    } else {
      m_cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
        layout->getPipelineLayout(), pushData.getStageMask(),
        pushInfo.offset, pushInfo.data.size, pushInfo.data.address);
    }
  }


  void DxvkContext::beginComputePass() {
    m_flags.set(DxvkContextFlag::CpComputePassActive);

    // Mark compute descriptors as dirty so that hazards are checked properly
    // between dispatches even when none of the resources were re-bound. This
    // can happen when a bound resource got written by a transfer op.
    m_descriptorState.clearStages(VK_SHADER_STAGE_ALL_GRAPHICS);
    m_descriptorState.dirtyStages(VK_SHADER_STAGE_COMPUTE_BIT);
  }


  void DxvkContext::endComputePass() {
    m_flags.clr(DxvkContextFlag::CpComputePassActive);
  }


  template<bool Indirect, bool Resolve>
  bool DxvkContext::commitComputeState() {
    this->endRenderPass(false);

    if (!m_flags.test(DxvkContextFlag::CpComputePassActive))
      this->beginComputePass();

    if (m_flags.any(DxvkContextFlag::CpDirtyPipelineState,
                    DxvkContextFlag::CpDirtySpecConstants)) {
      if (unlikely(!this->updateComputePipelineState()))
        return false;
    }

    if (this->checkComputeHazards<Indirect>()) {
      this->flushBarriers();

      // Dirty descriptors if this hasn't happened yet for
      // whatever reason in order to re-emit barriers
      m_descriptorState.dirtyStages(VK_SHADER_STAGE_COMPUTE_BIT);
    }

    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
      this->beginBarrierControlDebugRegion<VK_PIPELINE_BIND_POINT_COMPUTE>();

    if (m_descriptorState.hasDirtyResources(VK_SHADER_STAGE_COMPUTE_BIT)) {
      this->updateComputeShaderResources();

      if (unlikely(Resolve && m_implicitResolves.hasPendingResolves())) {
        this->flushImplicitResolves();
        return this->commitComputeState<Indirect, false>();
      }
    }

    if (m_flags.all(DxvkContextFlag::CpHasPushData,
                    DxvkContextFlag::DirtyPushData))
      this->updatePushData<VK_PIPELINE_BIND_POINT_COMPUTE>();

    return true;
  }
  
  
  template<bool Indexed, bool Indirect, bool Resolve>
  bool DxvkContext::commitGraphicsState() {
    if (m_flags.test(DxvkContextFlag::GpDirtyPipeline)) {
      if (unlikely(!this->updateGraphicsPipeline()))
        return false;
    }

    // End render pass if there are pending resolves
    if (m_flags.any(DxvkContextFlag::GpDirtyRenderTargets,
                    DxvkContextFlag::GpRenderPassNeedsFlush))
      this->updateRenderTargets();

    if (m_flags.test(DxvkContextFlag::GpXfbActive)) {
      // If transform feedback is active and there is a chance that we might
      // need to rebind the pipeline, we need to end transform feedback and
      // issue a barrier. End the render pass to do that. Ignore dirty vertex
      // buffers here since non-dynamic vertex strides are such an extreme
      // edge case that it's likely irrelevant in practice.
      if (m_flags.any(DxvkContextFlag::GpDirtyPipelineState,
                      DxvkContextFlag::GpDirtySpecConstants,
                      DxvkContextFlag::GpDirtyXfbBuffers)) {
        this->endCurrentPass(true);
        this->flushBarriers();
      }
    }

    // If a depth-stencil image is bound used with non-default sample locations,
    // make sure that the image actually has the compat flag set.
    if (unlikely(m_state.gp.state.useSampleLocations())) {
      if (m_state.om.renderTargets.depth.view) {
        VkSampleCountFlagBits samples = m_state.om.renderTargets.depth.view->image()->info().sampleCount;

        if (m_device->canUseSampleLocations(samples)) {
          auto flags = m_state.om.renderTargets.depth.view->image()->info().flags;

          if (!(flags & VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT)) {
            this->endCurrentPass(true);

            DxvkImageUsageInfo usage = { };
            usage.flags = VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;

            ensureImageCompatibility(m_state.om.renderTargets.depth.view->image(), usage);
          }
        }
      }
    }

    // Check whether we can actually start the render pass as unsynchronized.
    if (!m_flags.any(DxvkContextFlag::GpRenderPassActive)) {
      if (renderPassStartUnsynchronized()) {
        m_flags.set(DxvkContextFlag::GpRenderPassUnsynchronized);
        m_unsynchronizedDrawCount = 0u;
      }

      // Dirty all descriptor sets if there is no active render pass so that we
      // actually check all bound resources for hazards as necessary. Otherwise,
      // descriptor state will only be dirtied once we actually start the render
      // pass, which may be too late.
      if (!m_flags.any(DxvkContextFlag::GpRenderPassActive))
        m_descriptorState.dirtyStages(VK_SHADER_STAGE_ALL_GRAPHICS);
    }

    if (m_flags.any(DxvkContextFlag::GpRenderPassSideEffects,
                    DxvkContextFlag::GpRenderPassUnsynchronized)
     || m_state.gp.flags.any(DxvkGraphicsPipelineFlag::HasStorageDescriptors,
                             DxvkGraphicsPipelineFlag::HasTransformFeedback)) {
      // If either the current pipeline has side effects or if there are pending
      // writes from previous draws, check for hazards. This also tracks any
      // resources written for the first time, but does not emit any barriers
      // on its own so calling this outside a render pass is safe. This also
      // implicitly dirties all state for which we need to track resource access.
      //
      // If the render pass is currently unsynchronized, we need to manually
      // flush barriers afterwards. We will have done full resource tracking,
      // so skipping the global barrier is still fine in that case.
      if (this->checkGraphicsHazards<Indexed, Indirect>()) {
        this->endCurrentPass(true);
        this->flushBarriers();
      }

      // The render pass flag gets reset when the render pass ends, so set it late
      if (m_state.gp.flags.any(DxvkGraphicsPipelineFlag::HasStorageDescriptors,
                               DxvkGraphicsPipelineFlag::HasTransformFeedback))
        m_flags.set(DxvkContextFlag::GpRenderPassSideEffects);

      if (m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized)) {
        // In case we entered an unsynchronized render pass with no pending writes,
        // i.e. if we issued a barrier before the pass, we can trivially revert the
        // pass to the regularly synchronized mode if the render pass happens to be
        // big. This isn't common, but Ashes of the Singularity hits this case.
        //
        // Doing this is safe even in case shader writes are used, because we keep
        // the actual tracking info intact. On the other hand, we cannot safely do
        // this if there are any pending writes without issuing a barrier.
        if ((++m_unsynchronizedDrawCount == MaxUnsynchronizedDraws) && m_execBarriers.hasPendingAccess(vk::AccessWriteMask))
          m_flags.clr(DxvkContextFlag::GpRenderPassUnsynchronized);
      }
    }

    // Start the render pass. This must happen before any render state
    // is set up so that we can safely use secondary command buffers.
    if (!m_flags.test(DxvkContextFlag::GpRenderPassActive))
      this->beginRenderPass();

    // If there are any pending clears, record them now
    if (unlikely(!m_deferredClears.empty()))
      flushClearsInline();

    if (m_flags.test(DxvkContextFlag::GpRenderPassSideEffects)) {
      // Make sure that the debug label for barrier control
      // always starts within an active render pass
      if (unlikely(m_features.test(DxvkContextFeature::DebugUtils)))
        this->beginBarrierControlDebugRegion<VK_PIPELINE_BIND_POINT_GRAPHICS>();
    }

    if (m_flags.test(DxvkContextFlag::GpDirtyIndexBuffer) && Indexed)
      this->updateIndexBufferBinding();
    
    if (m_flags.test(DxvkContextFlag::GpDirtyVertexBuffers))
      this->updateVertexBufferBindings();
    
    if (m_flags.test(DxvkContextFlag::GpDirtySpecConstants))
      this->updateSpecConstants<VK_PIPELINE_BIND_POINT_GRAPHICS>();

    if (m_flags.test(DxvkContextFlag::GpDirtyPipelineState)) {
      if (unlikely(!this->updateGraphicsPipelineState()))
        return false;
    }
    
    if (m_descriptorState.hasDirtyResources(VK_SHADER_STAGE_ALL_GRAPHICS)) {
      if (unlikely(!this->updateGraphicsShaderResources())) {
        // This can only happen if we were inside a secondary command buffer.
        // Technically it would be sufficient to only restart the secondary
        // command buffer, but this should almost never happen in practice
        // anyway so avoid the complexity and just suspend the render pass.
        this->endCurrentPass(true);

        m_cmd->createDescriptorRange();

        return this->commitGraphicsState<Indexed, Indirect>();
      }

      if (unlikely(Resolve && m_implicitResolves.hasPendingResolves())) {
        // If implicit resolves are required for any of the shader bindings, we need
        // to discard all the state setup that we've done so far and try again
        this->endCurrentPass(true);
        this->flushImplicitResolves();

        return this->commitGraphicsState<Indexed, Indirect, false>();
      }
    }
    
    if (m_state.gp.flags.test(DxvkGraphicsPipelineFlag::HasTransformFeedback))
      this->updateTransformFeedbackState();
    
    this->updateDynamicState();
    
    if (m_flags.all(DxvkContextFlag::GpHasPushData, DxvkContextFlag::DirtyPushData))
      this->updatePushData<VK_PIPELINE_BIND_POINT_GRAPHICS>();

    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect)
      this->trackDrawBuffer();

    return true;
  }
  
  
  template<VkPipelineBindPoint BindPoint>
  bool DxvkContext::checkResourceHazards(
    const DxvkPipelineBindings*     layout) {
    // For performance reasons, we generall want to skip tracking resources that are
    // not written from any graphics shaders, but in unsynchronized passes we have
    // to check everything anyway.
    bool checkEverything = BindPoint == VK_PIPELINE_BIND_POINT_COMPUTE
      || m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized);

    // Iterate over all resources that are actively being written by the shader pipeline.
    // On graphics, this must not exit early since extra resource tracking is required.
    { auto range = layout->getReadWriteResources();

      if (range.bindingCount) {
        bool requiresBarrier = false;

        for (uint32_t j = 0u; j < range.bindingCount; j++) {
          const auto& binding = range.bindings[j];
          const auto& slot = m_resources[binding.getResourceIndex()];

          switch (binding.getDescriptorType()) {
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
              if (slot.bufferView) {
                if (checkEverything || slot.bufferView->buffer()->hasGfxStores()) {
                  requiresBarrier = requiresBarrier || checkBufferViewBarrier<BindPoint>(
                    slot.bufferView, binding.getAccess(), binding.getAccessOp());
                } else {
                  requiresBarrier = !slot.bufferView->buffer()->trackGfxStores() || requiresBarrier;
                }
              }
            } break;

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
              if (slot.imageView) {
                if (checkEverything || slot.imageView->hasGfxStores()) {
                  requiresBarrier = requiresBarrier || checkImageViewBarrier<BindPoint>(
                    slot.imageView, binding.getAccess(), binding.getAccessOp());
                } else {
                  requiresBarrier = !slot.imageView->image()->trackGfxStores() || requiresBarrier;
                }
              }
            } break;

            default:
              /* nothing to do */;
          }

          // On compute, we may exit immediately since no additional tracking is required.
          if (checkEverything && requiresBarrier)
            return true;
        }

        // Once we've processed all written resources, we can exit on graphics as well.
        // If we ever transition an unsynchronized pass to a synchronized pass, we will
        // have to issue a barrier since we may have skipped per-resource store tracking.
        if (!checkEverything && requiresBarrier)
          return true;
      }
    }

    // For read-only resources, it is sufficient to check dirty bindings since
    // any resource previously bound as read-only cannot have been written by
    // the same pipeline.
    VkShaderStageFlags dirtyStageMask = m_descriptorState.getDirtyStageMask(
      DxvkDescriptorClass::Buffer | DxvkDescriptorClass::View | DxvkDescriptorClass::Va);
    dirtyStageMask &= layout->getNonemptyStageMask();

    for (auto stageIndex : bit::BitMask(uint32_t(dirtyStageMask))) {
      VkShaderStageFlagBits stage = VkShaderStageFlagBits(1u << stageIndex);

      // Check any view-based resource for hazards
      auto range = layout->getReadOnlyResourcesForStage(stage);

      for (uint32_t j = 0; j < range.bindingCount; j++) {
        const auto& binding = range.bindings[j];

        switch (binding.getDescriptorType()) {
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            if (binding.isUniformBuffer()) {
              const auto& slot = m_uniformBuffers[binding.getResourceIndex()];

              if (slot.length() && (checkEverything || slot.buffer()->hasGfxStores())) {
                if (checkBufferBarrier<BindPoint>(slot, binding.getAccess(), DxvkAccessOp::None))
                  return true;
              }
              break;
            }
            [[fallthrough]];

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            const auto& slot = m_resources[binding.getResourceIndex()];

            if (slot.bufferView && (checkEverything || slot.bufferView->buffer()->hasGfxStores())) {
              if (checkBufferViewBarrier<BindPoint>(slot.bufferView, binding.getAccess(), DxvkAccessOp::None))
                return true;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            const auto& slot = m_resources[binding.getResourceIndex()];

            if (slot.imageView && (checkEverything || slot.imageView->hasGfxStores())) {
              if (checkImageViewBarrier<BindPoint>(slot.imageView, binding.getAccess(), DxvkAccessOp::None))
                return true;
            }
          } break;

          default:
            /* nothing to do */;
        }
      }
    }

    return false;
  }
  

  template<bool Indirect>
  bool DxvkContext::checkComputeHazards() {
    // Exit early if we know that there cannot be any hazards to avoid
    // some overhead after barriers are flushed. This is common.
    if (m_barrierTracker.empty())
      return false;

    bool requiresBarrier = checkResourceHazards<VK_PIPELINE_BIND_POINT_COMPUTE>(m_state.cp.pipeline->getLayout());

    if (Indirect && !requiresBarrier) {
      requiresBarrier = checkBufferBarrier<VK_PIPELINE_BIND_POINT_COMPUTE>(
        m_state.id.argBuffer, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, DxvkAccessOp::None);
    }

    return requiresBarrier;
  }


  template<bool Indexed, bool Indirect>
  bool DxvkContext::checkGraphicsHazards() {
    // If the current pipeline does not have any stores, we only need to iterate
    // over all resources if any stores are in the current barrier set. If the
    // render pass has side effects, we know that there are writes already.
    if (!m_flags.test(DxvkContextFlag::GpRenderPassSideEffects)
     && !m_state.gp.flags.any(DxvkGraphicsPipelineFlag::HasStorageDescriptors,
                              DxvkGraphicsPipelineFlag::HasTransformFeedback)
     && !m_execBarriers.hasLayoutTransitions()
     && !m_execBarriers.hasPendingAccess(vk::AccessWriteMask))
      return false;

    // Check shader resources on every draw to handle WAW hazards, and to make
    // sure that writes are handled properly. Checking dirty sets is sufficient
    // since we will unconditionally iterate over writable resources anyway.
    bool requiresBarrier = checkResourceHazards<VK_PIPELINE_BIND_POINT_GRAPHICS>(m_state.gp.pipeline->getLayout());

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

    bool unsynchronizedPass = m_flags.test(DxvkContextFlag::GpRenderPassUnsynchronized);

    // Check the draw buffer for indirect draw calls
    if (m_flags.test(DxvkContextFlag::DirtyDrawBuffer) && Indirect) {
      std::array<DxvkBufferSlice*, 2> slices = {{
        &m_state.id.argBuffer,
        &m_state.id.cntBuffer,
      }};

      for (uint32_t i = 0; i < slices.size(); i++) {
        if (slices[i]->length() && (unsynchronizedPass || slices[i]->buffer()->hasGfxStores())) {
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

      if (indexBufferSlice.length() && (unsynchronizedPass || indexBufferSlice.buffer()->hasGfxStores())) {
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

        if (vertexBufferSlice.length() && (unsynchronizedPass || vertexBufferSlice.buffer()->hasGfxStores())) {
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
      this->endCurrentPass(true);

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

    // Ensure that all images are in their default layout and are
    // safely accessible, but ignore any uninitialized subresources.
    std::vector<DxvkResourceAccess> accessBatch;

    for (size_t i = 0u; i < bufferCount; i++) {
      const auto& e = bufferInfos[i];

      accessBatch.emplace_back(*e.buffer, 0u, e.buffer->info().size,
        e.buffer->info().stages, e.buffer->info().access);
    }

    for (size_t i = 0u; i < imageCount; i++) {
      const auto& e = imageInfos[i];

      if (e.image->isInitialized(e.image->getAvailableSubresources())) {
        accessBatch.emplace_back(*e.image, e.image->getAvailableSubresources(),
          e.image->info().layout, e.image->info().stages, e.image->info().access, false);
      } else {
        VkImageAspectFlags aspects = e.image->formatInfo()->aspectMask;
        VkImageSubresource subresource = { };

        while (aspects) {
          subresource.aspectMask = vk::getNextAspect(aspects);

          for (subresource.mipLevel = 0u; subresource.mipLevel < e.image->info().mipLevels; subresource.mipLevel++) {
            for (subresource.arrayLayer = 0u; subresource.arrayLayer < e.image->info().numLayers; subresource.arrayLayer++) {
              if (e.image->isInitialized(subresource)) {
                accessBatch.emplace_back(*e.image, vk::makeSubresourceRange(subresource),
                  e.image->info().layout, e.image->info().stages, e.image->info().access, false);
              }
            }
          }
        }
      }
    }

    acquireResources(DxvkCmdBuffer::ExecBuffer, accessBatch.size(), accessBatch.data());

    // Use low-level barriers while processing actual copies
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

            if (info.image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
              dstBarrier.subresourceRange.baseArrayLayer = 0u;
              dstBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            }

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

            if (info.image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
              srcBarrier.subresourceRange.baseArrayLayer = 0u;
              srcBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            }

            imageBarriers.push_back(srcBarrier);
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
      m_cmd->track(info.buffer, DxvkAccess::Move);

      memoryBarrier.dstStageMask |= info.buffer->info().stages;
      memoryBarrier.dstAccessMask |= info.buffer->info().access;
    }

    // Copy and invalidate all images
    for (size_t i = 0; i < imageCount; i++) {
      const auto& info = imageInfos[i];
      auto oldStorage = info.image->storage();

      VkImageLayout finalLayout = info.usageInfo.layout ? info.usageInfo.layout : info.image->info().layout;

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
              dstBarrier.newLayout = finalLayout;
              dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
              dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
              dstBarrier.image = info.storage->getImageInfo().image;
              dstBarrier.subresourceRange = vk::makeSubresourceRange(region.dstSubresource);

              if (info.image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
                dstBarrier.subresourceRange.baseArrayLayer = 0u;
                dstBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
              }

              imageBarriers.push_back(dstBarrier);
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

      invalidateImageWithUsage(info.image, Rc<DxvkResourceAllocation>(info.storage), info.usageInfo, finalLayout);

      m_cmd->cmdCopyImage(DxvkCmdBuffer::ExecBuffer, &copy);
      m_cmd->track(info.image, DxvkAccess::Move);
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
    if (unlikely(m_features.test(DxvkContextFeature::DebugUtils))) {
      m_cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xc0a2f0, "Memory defrag"));
    }

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

    auto slice = m_zeroBuffer->getSliceInfo();

    // FillBuffer is allowed even on transfer queues. Execute it on the barrier
    // command buffer to ensure that subsequent transfer commands can see it.
    m_cmd->cmdFillBuffer(DxvkCmdBuffer::SdmaBarriers,
      slice.buffer, slice.offset, slice.size, 0);

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
    m_legacyDescriptors.infos.resize(bindingCount);
    m_legacyDescriptors.writes.resize(bindingCount);

    for (uint32_t i = 0; i < bindingCount; i++) {
      auto& info = m_legacyDescriptors.infos[i];

      auto& write = m_legacyDescriptors.writes[i];
      write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
      write.pImageInfo = &info.image;
      write.pBufferInfo = &info.buffer;
      write.pTexelBufferView = &info.bufferView;
    }
  }


  void DxvkContext::flushImplicitResolves() {
    endCurrentPass(true);

    DxvkImplicitResolveOp op;

    while (m_implicitResolves.extractResolve(op)) {
      // Always do a SAMPLE_ZERO resolve here since that's less expensive and closer to what
      // happens on native AMD anyway. Need to use a shader in case we are dealing with a
      // non-integer color image since render pass resolves only support AVERAGE..
      // We know that the source image is shader readable and that the resolve image can be
      // rendered to, so reduce the image compatibility checks to a minimum here.
      bool useRp = (getDefaultResolveMode(op.resolveFormat) == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) &&
        (op.inputImage->info().usage & (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

      if (useRp) {
        resolveImageRp(op.resolveImage, op.inputImage, op.resolveRegion,
          op.resolveFormat, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, true);
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
      DxvkContextFlag::GpRenderPassActive,
      DxvkContextFlag::GpXfbActive,
      DxvkContextFlag::GpIndependentSets,
      DxvkContextFlag::CpComputePassActive);

    m_flags.set(
      DxvkContextFlag::GpDirtyRenderTargets,
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::GpDirtyIndexBuffer,
      DxvkContextFlag::GpDirtyXfbBuffers,
      DxvkContextFlag::GpDirtyBlendConstants,
      DxvkContextFlag::GpDirtyStencilTest,
      DxvkContextFlag::GpDirtyStencilRef,
      DxvkContextFlag::GpDirtyMultisampleState,
      DxvkContextFlag::GpDirtyRasterizerState,
      DxvkContextFlag::GpDirtySampleLocations,
      DxvkContextFlag::GpDirtyViewport,
      DxvkContextFlag::GpDirtyDepthBias,
      DxvkContextFlag::GpDirtyDepthBounds,
      DxvkContextFlag::GpDirtyDepthClip,
      DxvkContextFlag::GpDirtyDepthTest,
      DxvkContextFlag::CpDirtyPipelineState,
      DxvkContextFlag::DirtyDrawBuffer);

    m_descriptorState.dirtyStages(
      VK_SHADER_STAGE_ALL_GRAPHICS |
      VK_SHADER_STAGE_COMPUTE_BIT);

    m_state.gp.pipeline = nullptr;
    m_state.cp.pipeline = nullptr;

    m_cmd->setTrackingId(++m_trackingId);

    if (m_features.any(DxvkContextFeature::DescriptorHeap,
                       DxvkContextFeature::DescriptorBuffer)) {
      m_cmd->setDescriptorHeap(m_descriptorHeap);
    } else {
      m_cmd->setDescriptorPool(m_descriptorPool);
    }
  }


  void DxvkContext::endCurrentCommands() {
    endCurrentPass(true);

    prepareSharedImages();

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

    if (image.info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
      barrier.subresourceRange.baseArrayLayer = 0u;
      barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }
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


  void DxvkContext::trackNonDefaultImageLayout(
            DxvkImage&                image) {
    for (const auto& e : m_nonDefaultLayoutImages) {
      if (e == &image)
        return;
    }

    m_nonDefaultLayoutImages.emplace_back(&image);
  }


  bool DxvkContext::overlapsRenderTarget(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources) {
    if (!(image.info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)))
      return false;

    if (image.info().usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      const auto& view = m_state.om.renderTargets.depth.view;

      if (view && view->image() == &image) {
        VkImageSubresourceRange viewSubresources = view->imageSubresources();

        if ((subresources.aspectMask & viewSubresources.aspectMask)
         && vk::checkSubresourceRangeOverlap(subresources, viewSubresources))
          return true;
      }
    }

    if (image.info().usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      for (uint32_t i = 0u; i < MaxNumRenderTargets; i++) {
        const auto& view = m_state.om.renderTargets.color[i].view;

        if (view && view->image() == &image) {
          VkImageSubresourceRange viewSubresources = view->imageSubresources();

          if ((subresources.aspectMask & viewSubresources.aspectMask)
           && vk::checkSubresourceRangeOverlap(subresources, viewSubresources))
            return true;
        }
      }
    }

    return false;
  }


  bool DxvkContext::restoreImageLayout(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          bool                      keepAttachments) {
    // Note that this method does not flush layout transitions
    m_cmd->track(&image, DxvkAccess::Read);

    if (likely(!keepAttachments || !overlapsRenderTarget(image, subresources))) {
      // Transition entire image to its default limit in one go
      transitionImageLayout(image, subresources,
        image.info().stages, image.info().access, image.info().layout,
        image.info().stages, image.info().access, false);
      return true;
    } else {
      // Iterate over each plane and mip level, and check whether
      // there is any overlap with bound render targets
      VkImageAspectFlags aspects = image.formatInfo()->aspectMask;

      while (aspects) {
        VkImageSubresourceRange range = { };
        range.aspectMask = vk::getNextAspect(aspects);

        for (uint32_t m = 0u; m < subresources.levelCount; m++) {
          range.baseMipLevel = subresources.baseMipLevel + m;
          range.levelCount = 1u;
          range.baseArrayLayer = subresources.baseArrayLayer;
          range.layerCount = subresources.layerCount;

          if (overlapsRenderTarget(image, range)) {
            // Scan and transition array layers one by one
            for (uint32_t l = 0u; l < subresources.layerCount; l++) {
              range.baseArrayLayer = subresources.baseArrayLayer + l;
              range.layerCount = 1u;

              if (!overlapsRenderTarget(image, range)) {
                transitionImageLayout(image, range,
                  image.info().stages, image.info().access, image.info().layout,
                  image.info().stages, image.info().access, false);
              }
            }
          } else {
            // Transition entire mip level at once
            transitionImageLayout(image, range,
              image.info().stages, image.info().access, image.info().layout,
              image.info().stages, image.info().access, false);
          }
        }
      }

      return false;
    }
  }


  template<typename Pred>
  void DxvkContext::restoreImageLayouts(const Pred& pred, bool keepAttachments) {
    if (m_nonDefaultLayoutImages.empty())
      return;

    size_t src = 0u;
    size_t dst = 0u;

    while (src < m_nonDefaultLayoutImages.size()) {
      bool fullyRestored = false;

      if (pred(*m_nonDefaultLayoutImages[src])) {
        fullyRestored = restoreImageLayout(*m_nonDefaultLayoutImages[src],
          m_nonDefaultLayoutImages[src]->getAvailableSubresources(),
          keepAttachments);
      }

      if (!fullyRestored) {
        if (dst < src)
          m_nonDefaultLayoutImages[dst] = std::move(m_nonDefaultLayoutImages[src]);

        dst += 1u;
      }

      src += 1u;
    }

    if (dst < src) {
      m_nonDefaultLayoutImages.resize(dst);

      flushImageLayoutTransitions(DxvkCmdBuffer::ExecBuffer);
    }
  }


  void DxvkContext::prepareShaderReadableImages(bool renderPass) {
    // Flush pending clears and emit barriers before potentially
    // emitting layout transitions affecting the same images.
    if (!m_deferredClears.empty()) {
      flushClears(renderPass);
      flushBarriers();
    }

    // Just transition all images for the time being, we can make this
    // more granular for tilers as necessary. When changing this, we will
    // also need to ensure that we don't keep destroyed images alive.
    if (!m_nonDefaultLayoutImages.empty()) {
      flushBarriers();

      restoreImageLayouts([] (DxvkImage& image) {
        return true;
      }, renderPass);
    }
  }


  void DxvkContext::prepareSharedImages() {
    // Only flush clears for shared images, and restore layouts
    bool hasSharedClear = false;

    for (const auto& image : m_nonDefaultLayoutImages) {
      if (image->info().shared)
        hasSharedClear = flushDeferredClear(*image, image->getAvailableSubresources());
    }

    if (hasSharedClear)
      flushBarriers();

    restoreImageLayouts([] (DxvkImage& image) {
      return image.info().shared;
    }, false);
  }


  bool DxvkContext::transitionImageLayout(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          VkPipelineStageFlags2     srcStages,
          VkAccessFlags2            srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags2     dstStages,
          VkAccessFlags2            dstAccess,
          bool                      discard) {
    // If all subresources are in the correct layout, we don't need to do anything.
    // If we're discarding a render target, re-initialize the image since doing so
    // may lead to more efficient compression in some cases.
    VkImageUsageFlags rtUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                              | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!discard || !(image.info().usage & rtUsage))
      srcLayout = image.queryLayout(subresources);

    if (likely(srcLayout == dstLayout))
      return false;

    if (srcLayout == VK_IMAGE_LAYOUT_MAX_ENUM) {
      VkImageAspectFlags aspects = subresources.aspectMask;
      VkImageSubresource subresource = { };

      while (aspects) {
        subresource.aspectMask = vk::getNextAspect(aspects);
        subresource.mipLevel = subresources.baseMipLevel;

        for (uint32_t m = 0u; m < subresources.levelCount; m++) {
          subresource.arrayLayer = subresources.baseArrayLayer;

          for (uint32_t l = 0u; l < subresources.layerCount; l++) {
            srcLayout = image.queryLayout(subresource);

            if (srcLayout != dstLayout) {
              addImageLayoutTransition(image, vk::makeSubresourceRange(subresource),
                srcLayout, srcStages, srcAccess,
                dstLayout, dstStages, dstAccess);
            }

            subresource.arrayLayer += 1u;
          }

          subresource.mipLevel += 1u;
        }
      }
    } else {
      addImageLayoutTransition(image, subresources,
        srcLayout, srcStages, srcAccess,
        dstLayout, dstStages, dstAccess);
    }

    if (dstLayout != image.info().layout)
      trackNonDefaultImageLayout(image);

    image.trackLayout(subresources, dstLayout);

    // Need to track for writes here even if
    // the actual access itself is read-only
    m_cmd->track(&image, DxvkAccess::Write);
    return true;
  }


  void DxvkContext::acquireResources(
          DxvkCmdBuffer             cmdBuffer,
          size_t                    count,
    const DxvkResourceAccess*       batch,
          bool                      flushClears) {
    if (cmdBuffer == DxvkCmdBuffer::InitBuffer)
      cmdBuffer = DxvkCmdBuffer::InitBarriers;
    else if (cmdBuffer == DxvkCmdBuffer::SdmaBuffer)
      cmdBuffer = DxvkCmdBuffer::SdmaBarriers;

    // For out-of-order operations we know that the resource can't have
    // any pending accesses that would require synchronization.
    bool needsFlush = cmdBuffer != DxvkCmdBuffer::ExecBuffer;

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer && flushClears) {
      // If we need to flush any clears, we need to so *before* modifying any
      // global state since clears themselves may need to issue barriers.
      for (size_t i = 0u; i < count; i++) {
        if (batch[i].image)
          needsFlush |= flushDeferredClear(*batch[i].image, batch[i].image->getAvailableSubresources());
      }
    }

    // Even if we have to perform the current operation on the main command buffer,
    // we can still try to move layout transitions that may be necessary to an
    // out-of-order command buffer in order to avoid additional barriers.
    bool promoteTransitions = m_imageLayoutTransitions.empty();

    // Flush any barriers affecting the resources
    VkPipelineStageFlags2 srcStages = 0u;
    VkPipelineStageFlags2 dstStages = 0u;

    VkAccessFlags2 srcAccess = 0u;
    VkAccessFlags2 dstAccess = 0u;

    for (size_t i = 0u; i < count; i++) {
      const auto& e = batch[i];

      DxvkAccess access = (e.access & vk::AccessWriteMask)
        ? DxvkAccess::Write
        : DxvkAccess::Read;

      if (e.buffer) {
        if (!needsFlush) {
          needsFlush = resourceHasAccess(*e.buffer, e.bufferOffset, e.bufferSize,
            DxvkAccess::Write, DxvkAccessOp::None);

          if (!needsFlush && (e.access & vk::AccessWriteMask)) {
            needsFlush = resourceHasAccess(*e.buffer, e.bufferOffset, e.bufferSize,
              DxvkAccess::Read, DxvkAccessOp::None);
          }
        }

        if (unlikely(e.stages & ~e.buffer->info().stages)
         || unlikely(e.access & ~e.buffer->info().access)) {
          srcStages |= e.buffer->info().stages;
          srcAccess |= e.buffer->info().access;
          dstStages |= e.stages;
          dstAccess |= e.access;
        }

        m_cmd->track(e.buffer, access);
      } else if (e.image) {
        if (!needsFlush) {
          if (!e.imageExtent.width) {
            if (!needsFlush) {
              needsFlush = resourceHasAccess(*e.image, e.imageSubresources,
                DxvkAccess::Write, DxvkAccessOp::None);

              if (!needsFlush && (e.access & vk::AccessWriteMask)) {
                needsFlush = resourceHasAccess(*e.image, e.imageSubresources,
                  DxvkAccess::Read, DxvkAccessOp::None);
              }
            }
          } else {
            VkImageSubresourceLayers layers = vk::pickSubresourceLayers(e.imageSubresources, 0u);

            needsFlush = resourceHasAccess(*e.image, layers,
              e.imageOffset, e.imageExtent, DxvkAccess::Write, DxvkAccessOp::None);

            if (!needsFlush && (e.access & vk::AccessWriteMask)) {
              needsFlush = resourceHasAccess(*e.image, layers,
                e.imageOffset, e.imageExtent, DxvkAccess::Read, DxvkAccessOp::None);
            }
          }
        }

        if (unlikely(e.stages & ~e.image->info().stages)
         || unlikely(e.access & ~e.image->info().access)) {
          srcStages |= e.image->info().stages;
          srcAccess |= e.image->info().access;
          dstStages |= e.stages;
          dstAccess |= e.access;
        }

        bool canPromote = !e.image->isTracked(m_trackingId, DxvkAccess::Write);

        bool hasTransition = transitionImageLayout(*e.image, e.imageSubresources,
          e.image->info().stages, e.image->info().access,
          e.imageLayout, e.stages, e.access, e.discard);

        if (hasTransition && !canPromote)
          promoteTransitions = false;

        m_cmd->track(e.image, access);
      }
    }

    // If the resource can be accessed in ways that regular barriers
    // don't handle, emit a global memory barrier to sort it out
    if (unlikely(srcStages | dstStages)) {
      accessMemory(cmdBuffer, srcStages, srcAccess, dstStages, dstAccess);
      needsFlush = true;
    }

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer && needsFlush)
      flushBarriers();

    // Move layout transitions and discards to init command buffer if possible
    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer && promoteTransitions)
      cmdBuffer = DxvkCmdBuffer::InitBarriers;

    flushImageLayoutTransitions(cmdBuffer);
  }


  void DxvkContext::releaseResources(
          DxvkCmdBuffer             cmdBuffer,
          size_t                    count,
    const DxvkResourceAccess*       batch) {
    for (size_t i = 0u; i < count; i++) {
      const auto& e = batch[i];

      if (e.buffer) {
        accessBuffer(cmdBuffer, *e.buffer, e.bufferOffset, e.bufferSize,
          e.stages, e.access, e.buffer->info().stages, e.buffer->info().access,
          DxvkAccessOp::None);
      } else if (e.image) {
        if (!e.imageExtent.width) {
          accessImage(cmdBuffer, *e.image, e.imageSubresources,
            e.imageLayout, e.stages, e.access, e.imageLayout,
            e.image->info().stages, e.image->info().access,
            DxvkAccessOp::None);
        } else {
          accessImageRegion(cmdBuffer, *e.image,
            vk::pickSubresourceLayers(e.imageSubresources, 0u),
            e.imageOffset, e.imageExtent, e.imageLayout,
            e.stages, e.access, e.imageLayout,
            e.image->info().stages, e.image->info().access,
            DxvkAccessOp::None);
        }
      }
    }
  }


  void DxvkContext::syncResources(
          DxvkCmdBuffer             cmdBuffer,
          size_t                    count,
    const DxvkResourceAccess*       batch,
          bool                      flushClears) {
    acquireResources(cmdBuffer, count, batch, flushClears);
    releaseResources(cmdBuffer, count, batch);
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

    // maintenance9 changed semantics for barriers involving 3D images
    if (image.info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
      barrier.subresourceRange.baseArrayLayer = 0u;
      barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }

    batch.addImageBarrier(barrier);

    if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) {
      bool hasWrite = (srcAccess & vk::AccessWriteMask) || (srcLayout != dstLayout);
      bool hasRead = (srcAccess & vk::AccessReadMask);

      uint32_t layerCount = image.info().numLayers;

      DxvkAddressRange range;
      range.resource = image.getResourceId();
      range.accessOp = accessOp;

      if (subresources.levelCount == 1u || subresources.layerCount == layerCount) {
        range.rangeStart = image.getSubresourceStartAddress(
          subresources.baseMipLevel, subresources.baseArrayLayer);
        range.rangeEnd = image.getSubresourceEndAddress(
          subresources.baseMipLevel + subresources.levelCount - 1u,
          subresources.baseArrayLayer + subresources.layerCount - 1u);

        if (hasWrite)
          m_barrierTracker.insertRange(range, DxvkAccess::Write);
        if (hasRead)
          m_barrierTracker.insertRange(range, DxvkAccess::Read);
      } else {
        for (uint32_t i = subresources.baseMipLevel; i < subresources.baseMipLevel + subresources.levelCount; i++) {
          range.rangeStart = image.getSubresourceStartAddress(i, subresources.baseArrayLayer);
          range.rangeEnd = image.getSubresourceEndAddress(i, subresources.baseArrayLayer + subresources.layerCount - 1u);

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
        range.rangeStart = image.getSubresourceStartAddress(subresources.mipLevel, subresources.baseArrayLayer);
        range.rangeEnd = image.getSubresourceEndAddress(subresources.mipLevel, subresources.baseArrayLayer + subresources.layerCount - 1u);

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
          range.rangeStart = image.getSubresourceAddressAt(subresources.mipLevel, i, offset);
          range.rangeEnd = image.getSubresourceAddressAt(subresources.mipLevel, i, maxCoord);

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

      if (image.info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
        barrier.subresourceRange.baseArrayLayer = 0u;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
      }

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

      if (image.info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
        barrier.subresourceRange.baseArrayLayer = 0u;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
      }

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
    range.rangeStart = image.getSubresourceStartAddress(
      subresources.baseMipLevel, subresources.baseArrayLayer);
    range.rangeEnd = image.getSubresourceEndAddress(
      subresources.baseMipLevel + subresources.levelCount - 1u,
      subresources.baseArrayLayer + subresources.layerCount - 1u);

    // Probe all subresources first, only check individual mip levels
    // if there are overlaps and if we are checking a subset of array
    // layers of multiple mips.
    bool dirty = m_barrierTracker.findRange(range, access);

    if (!dirty || subresources.levelCount == 1u || subresources.layerCount == layerCount)
      return dirty;

    for (uint32_t i = subresources.baseMipLevel; i < subresources.baseMipLevel + subresources.levelCount && !dirty; i++) {
      range.rangeStart = image.getSubresourceStartAddress(i, subresources.baseArrayLayer);
      range.rangeEnd = image.getSubresourceEndAddress(i, subresources.baseArrayLayer + subresources.layerCount - 1u);

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
      range.rangeStart = image.getSubresourceStartAddress(subresources.mipLevel, subresources.baseArrayLayer);
      range.rangeEnd = image.getSubresourceEndAddress(subresources.mipLevel, subresources.baseArrayLayer + subresources.layerCount - 1u);

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
      range.rangeStart = image.getSubresourceAddressAt(subresources.mipLevel, i, offset);
      range.rangeEnd = image.getSubresourceAddressAt(subresources.mipLevel, i, maxCoord);

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


  DxvkCmdBuffer DxvkContext::prepareOutOfOrderTransfer(
          DxvkCmdBuffer             cmdBuffer,
          size_t                    accessCount,
    const DxvkResourceAccess*       accessBatch) {
    for (size_t i = 0u; i < accessCount; i++) {
      const auto& e = accessBatch[i];

      DxvkAccess access = (e.access & vk::AccessWriteMask)
        ? DxvkAccess::Write
        : DxvkAccess::Read;

      if (e.buffer) {
        if (!prepareOutOfOrderTransfer(*e.buffer, e.bufferOffset, e.bufferSize, access))
          return DxvkCmdBuffer::ExecBuffer;
      } else if (e.image) {
        if (!prepareOutOfOrderTransfer(*e.image, e.imageSubresources, e.discard, access))
          return DxvkCmdBuffer::ExecBuffer;
      }
    }

    return cmdBuffer;
  }


  bool DxvkContext::prepareOutOfOrderTransfer(
          DxvkBuffer&               buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          DxvkAccess                access) {
    // Sparse resources can alias, need to ignore.
    if (unlikely(buffer.info().flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT))
      return false;

    // If the resource hasn't been used yet or both uses are reads,
    // we can use this buffer in the init command buffer
    if (!buffer.isTracked(m_trackingId, access))
      return true;

    // Otherwise, our only option is to discard. We can only do that if
    // we're writing the full buffer. Therefore, the resource being read
    // should always be checked first to avoid unnecessary discards.
    if (access != DxvkAccess::Write || size < buffer.info().size || offset)
      return false;

    // Check if the buffer can actually be discarded at all.
    if (!buffer.canRelocate())
      return false;

    // Ignore large buffers to keep memory overhead in check. Use a higher
    // threshold when a render pass is active to avoid interrupting it.
    VkDeviceSize threshold = !m_flags.test(DxvkContextFlag::GpRenderPassActive)
      ? MaxDiscardSizeInRp
      : MaxDiscardSize;

    if (size > threshold)
      return false;

    // If the buffer is used for transform feedback in any way, we have to stop
    // the render pass anyway, but we can at least avoid an extra barrier.
    if (unlikely(m_flags.test(DxvkContextFlag::GpXfbActive))) {
      VkBufferUsageFlags xfbUsage = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT
                                  | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;

      if (buffer.info().usage & xfbUsage)
        this->endCurrentPass(true);
    }

    // Actually allocate and assign new backing storage
    this->invalidateBuffer(&buffer, buffer.allocateStorage());
    return true;
  }


  bool DxvkContext::prepareOutOfOrderTransfer(
          DxvkImage&                image,
    const VkImageSubresourceRange&  subresources,
          bool                      discard,
          DxvkAccess                access) {
    // Sparse resources can alias, need to ignore.
    if (unlikely(image.info().flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT))
      return false;

    // Reject any images that use non-default image layouts since
    // per-subresource layout tracking relies on proper ordering
    if (unlikely(!image.hasUnifiedLayout()))
      return false;

    // Ensure correct order of operations in case the image is a render
    // target and is either currently bound for rendering or has any
    // pending clears or resolves.
    if (image.info().usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      if (findOverlappingDeferredClear(image, subresources)
       || findOverlappingDeferredResolve(image, subresources))
        return false;

      if (m_flags.test(DxvkContextFlag::GpRenderPassActive)) {
        if (isBoundAsRenderTarget(image, subresources))
          return false;
      }
    }

    // If the image hasn't been used yet or all uses are reads,
    // we can use it in the init command buffer. Treat discards
    // as a write since we will reinitialize the image.
    if (discard)
      access = DxvkAccess::Write;

    return !image.isTracked(m_trackingId, access);
  }


  bool DxvkContext::prepareOutOfOrderTransition(
          DxvkImage&                image) {
    // Sparse resources can alias, need to ignore.
    return !(image.isTracked(m_trackingId, DxvkAccess::Write))
        && !(image.info().flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT);
  }


  VkStencilOpState DxvkContext::convertStencilOp(
    const DxvkStencilOp&            op,
          bool                      writable) {
    VkStencilOpState result = { };
    result.compareOp = op.compareOp();
    result.compareMask = op.compareMask();

    if (writable) {
      result.failOp = op.failOp();
      result.passOp = op.passOp();
      result.depthFailOp = op.depthFailOp();
      result.writeMask = op.writeMask();
    }

    return result;
  }


  bool DxvkContext::formatsAreBufferCopyCompatible(
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


  bool DxvkContext::formatsAreImageCopyCompatible(
          VkFormat                  dstFormat,
          VkFormat                  srcFormat) {
    // Assume compatibility as long as no depth/stencil formats
    // are involved, or if the aspect masks are equal
    constexpr VkImageAspectFlags DsAspects = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    auto dstAspects = lookupFormatInfo(dstFormat)->aspectMask;
    auto srcAspects = lookupFormatInfo(srcFormat)->aspectMask;

    if (srcAspects == dstAspects || !((srcAspects | dstAspects) & DsAspects))
      return true;

    // If maintenance8 is enabled on the device, we can use copy functions
    // directly when copying between certain formats. Don't bother with
    // stencil since depth-stencil aspects are interleaved in our model.
    if (m_device->features().khrMaintenance8.maintenance8) {
      auto depthFormat = (dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT) ? dstFormat : srcFormat;
      auto colorFormat = (dstAspects & VK_IMAGE_ASPECT_COLOR_BIT) ? dstFormat : srcFormat;

      if (depthFormat == VK_FORMAT_D16_UNORM) {
        return colorFormat == VK_FORMAT_R16_SFLOAT
            || colorFormat == VK_FORMAT_R16_UNORM
            || colorFormat == VK_FORMAT_R16_UINT;
      }

      if (depthFormat == VK_FORMAT_D32_SFLOAT) {
        return colorFormat == VK_FORMAT_R32_SFLOAT
            || colorFormat == VK_FORMAT_R32_UINT;
      }
    }

    return false;
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
        return srcFormat;
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


  DxvkResourceBufferInfo DxvkContext::allocateScratchMemory(
          VkDeviceSize                alignment,
          VkDeviceSize                size) {
    if (unlikely(!m_scratchBuffer || m_scratchBuffer->info().size < size)) {
      // We probably won't need a lot of scratch memory, so keep it small
      DxvkBufferCreateInfo info = { };
      info.size = DxvkPageAllocator::PageSize;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                 | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                 | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      info.stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT
                  | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      info.access = VK_ACCESS_2_TRANSFER_READ_BIT
                  | VK_ACCESS_2_TRANSFER_WRITE_BIT
                  | VK_ACCESS_2_SHADER_READ_BIT
                  | VK_ACCESS_2_SHADER_WRITE_BIT;
      info.debugName = "Scratch buffer";

      m_scratchBuffer = m_device->createBuffer(info,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    // Suballocate slice from buffer
    VkDeviceSize offset = align(m_scratchOffset, alignment);

    if (offset + size < m_scratchBuffer->info().size) {
      invalidateBuffer(m_scratchBuffer, m_scratchBuffer->allocateStorage());
      m_scratchOffset = 0u;
    }

    DxvkResourceBufferInfo slice = m_scratchBuffer->getSliceInfo(m_scratchOffset, size);
    m_scratchOffset += align(size, alignment);

    // Unconditionally track for writing to not bother the caller with it.
    m_cmd->track(m_scratchBuffer, DxvkAccess::Write);
    return slice;
  }

}
