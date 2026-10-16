
#include "d3d9_device.h"
#include "d3d9_interface.h"
#include "d3d9_bridge.h"
#include "d3d9_swapchain.h"
#include "d3d9_surface.h"
#include "d3d9_format.h"

namespace dxvk {

  DxvkLegacyD3DDeviceBridge::DxvkLegacyD3DDeviceBridge(D3D9DeviceEx* pDevice)
    : m_device(pDevice) {
  }

  DxvkLegacyD3DDeviceBridge::~DxvkLegacyD3DDeviceBridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::AddRef() {
    return m_device->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkLegacyD3DDeviceBridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  HRESULT DxvkLegacyD3DDeviceBridge::CopyRects(
          IDirect3DSurface9*  pSourceSurface,
    const RECT*               pSourceRectsArray,
          UINT                cRects,
          IDirect3DSurface9*  pDestinationSurface,
    const POINT*              pDestPointsArray) {
    D3D9DeviceLock lock = m_device->LockDevice();

    // The D3D8 side already checks for NULLs and Src == Dst.

    D3D9Surface* src = static_cast<D3D9Surface*>(pSourceSurface);
    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestinationSurface);

    D3D9CommonTexture* srcTex = src->GetCommonTexture();
    D3D9CommonTexture* dstTex = dst->GetCommonTexture();

    VkExtent3D srcTexLevelExtent = srcTex->GetExtentMip(src->GetMipLevel());
    VkExtent3D dstTexLevelExtent = dstTex->GetExtentMip(dst->GetMipLevel());

    // This function does not support format conversion.
    if (unlikely(srcTex->Desc()->Format != dstTex->Desc()->Format))
      return D3DERR_INVALIDCALL;

    // This function cannot be applied to surfaces whose formats are classified as depth stencil formats.
    if (unlikely(IsDepthStencilFormat(dstTex->Desc()->Format)))
      return D3DERR_INVALIDCALL;

    if (unlikely(dstTex->GetFormatMapping().ConversionFormatInfo.FormatType != D3D9ConversionFormat_None
             && (srcTex->GetImage() != nullptr && dstTex->GetImage() == nullptr))) {
      Logger::err("D3D8Device::CopyRects: Formats that need conversion are only supported on image to image copies");
      return D3DERR_NOTAVAILABLE;
    }

    // If pSourceRectsArray is NULL, then the entire surface is copied
    RECT rect = { 0, 0, int32_t(srcTexLevelExtent.width), int32_t(srcTexLevelExtent.height) };
    POINT point = { 0, 0 };
    if (pSourceRectsArray == nullptr) {
      cRects = 1;
      pSourceRectsArray = &rect;
      pDestPointsArray = &point;
    }

    auto formatInfo = lookupFormatInfo(dstTex->GetFormatMapping().Format);

    // Validate all rects & points
    for (uint32_t i = 0; i < cRects; i++) {
      RECT srcRect = pSourceRectsArray[i];
      POINT dstPoint;
      if (pDestPointsArray != nullptr) {
        dstPoint = pDestPointsArray[i];
      } else {
        dstPoint = { srcRect.left, srcRect.top };
      }

      if (srcRect.left < 0 || srcRect.top < 0
        || srcRect.left >= srcRect.right
        || srcRect.top >= srcRect.bottom
        || uint32_t(srcRect.right) > srcTexLevelExtent.width
        || uint32_t(srcRect.bottom) > srcTexLevelExtent.height
        || (srcRect.left % formatInfo->blockSize.width) != 0
        || (srcRect.right % formatInfo->blockSize.width) != 0
        || (srcRect.top % formatInfo->blockSize.height) != 0
        || (srcRect.bottom % formatInfo->blockSize.height) != 0) {
        return D3DERR_INVALIDCALL;
      }

      VkExtent2D extent = { uint32_t(srcRect.right - srcRect.left), uint32_t(srcRect.bottom - srcRect.top) };
      if (dstPoint.x < 0 || dstPoint.y < 0
        || dstPoint.x + extent.width > dstTexLevelExtent.width
        || dstPoint.y + extent.height > dstTexLevelExtent.height
        || (dstPoint.x % formatInfo->blockSize.width) != 0
        || (dstPoint.y % formatInfo->blockSize.height) != 0) {
        return D3DERR_INVALIDCALL;
      }
    }

    bool srcHasImage = srcTex->GetImage() != nullptr;
    bool dstHasImage = dstTex->GetImage() != nullptr;

    // Flush remaining managed texture uploads.
    if (srcHasImage && srcTex->NeedsUpload(src->GetSubresource()))
      m_device->FlushImage(srcTex, src->GetSubresource());
    if (dstHasImage && dstTex->NeedsUpload(dst->GetSubresource()))
      m_device->FlushImage(dstTex, dst->GetSubresource());

    for (uint32_t i = 0; i < cRects; i++) {
      RECT srcRect;
      POINT dstPoint;
      srcRect = pSourceRectsArray[i];
      if (pDestPointsArray != nullptr) {
        dstPoint = pDestPointsArray[i];
      } else {
        dstPoint = { srcRect.left, srcRect.top };
      }

      VkOffset3D dstOffset = { dstPoint.x, dstPoint.y, 0};
      VkOffset3D srcOffset = { srcRect.left, srcRect.top, 0 };
      VkExtent3D extent = { uint32_t(srcRect.right - srcRect.left), uint32_t(srcRect.bottom - srcRect.top), 1 };

      VkExtent3D blockCount = util::computeBlockCount(extent, formatInfo->blockSize);
      VkOffset3D srcOffsetBlockCount = util::computeBlockOffset(srcOffset, formatInfo->blockSize);
      VkOffset3D dstOffsetBlockCount = util::computeBlockOffset(dstOffset, formatInfo->blockSize);
      VkExtent3D srcTexLevelExtentBlockCount = util::computeBlockCount(srcTexLevelExtent, formatInfo->blockSize);
      VkExtent3D dstTexLevelExtentBlockCount = util::computeBlockCount(dstTexLevelExtent, formatInfo->blockSize);
      VkDeviceSize srcPitch = align(srcTexLevelExtentBlockCount.width * formatInfo->elementSize, 4);
      VkDeviceSize dstPitch = align(dstTexLevelExtentBlockCount.width * formatInfo->elementSize, 4);
      VkDeviceSize bufferSrcOffset = srcOffsetBlockCount.y * srcPitch + srcOffsetBlockCount.x * formatInfo->elementSize;
      VkDeviceSize bufferDstOffset = dstOffsetBlockCount.y * dstPitch + dstOffsetBlockCount.x * formatInfo->elementSize;

      if (srcHasImage && dstHasImage) {
        CopyRectImageToImage(srcTex, dstTex, src->GetSubresource(), dst->GetSubresource(),
          srcOffset, dstOffset, extent);
      }
      else if (!srcHasImage && dstHasImage) {
        // Just use the staging buffer upload here.
        m_device->UpdateTextureFromBuffer(dstTex, srcTex, dst->GetSubresource(), src->GetSubresource(),
          srcOffset, extent, dstOffset);
      }
      else if (srcHasImage && !dstHasImage) {
        CopyRectImageToBuffer(srcTex, dstTex, src->GetSubresource(), dst->GetSubresource(),
          srcOffset, bufferDstOffset, dstPitch, extent, formatInfo);
      }
      else if (!srcHasImage && !dstHasImage) {
        CopyRectBufferToBuffer(srcTex, dstTex, src->GetSubresource(), dst->GetSubresource(),
          bufferSrcOffset, bufferDstOffset, srcPitch, dstPitch, blockCount, formatInfo);
      }
    }

    // We do GPU copies to the DST unless both src and dst don't have images.
    if (srcHasImage || dstHasImage)
      dstTex->SetNeedsReadback(dst->GetSubresource(), true);

    if (dstTex->IsAutomaticMip())
      m_device->MarkTextureMipsDirty(dstTex);

    return D3D_OK;
  }

  void DxvkLegacyD3DDeviceBridge::CopyRectImageToImage(
    const D3D9CommonTexture* SrcTex, D3D9CommonTexture* DstTex,
    uint32_t SrcSubresource, uint32_t DstSubresource,
    VkOffset3D SrcOffset, VkOffset3D DstOffset, VkExtent3D Extent) {
      // The backend will thankfully handle all VkUsage issues.

      const Rc<DxvkImage>& srcImage = SrcTex->GetImage();
      const Rc<DxvkImage>& dstImage = DstTex->GetImage();

      VkImageSubresourceLayers srcLayers = vk::makeSubresourceLayers(SrcTex->GetSubresourceFromIndex(
        VK_IMAGE_ASPECT_COLOR_BIT, SrcSubresource));

      VkImageSubresourceLayers dstLayers = vk::makeSubresourceLayers(DstTex->GetSubresourceFromIndex(
        VK_IMAGE_ASPECT_COLOR_BIT, DstSubresource));

      if (dstImage->info().sampleCount == srcImage->info().sampleCount
        || dstImage->info().sampleCount == 1u) {
        m_device->EmitCs([
          cSrcImage  = srcImage,
          cDstImage  = dstImage,
          cSrcOffset = SrcOffset,
          cDstOffset = DstOffset,
          cSrcLayers = srcLayers,
          cDstLayers = dstLayers,
          cExtent    = Extent,
          cResolve   = srcImage->info().sampleCount != dstImage->info().sampleCount
        ] (DxvkContext* ctx) {
          if (!cResolve) {
            ctx->copyImage(cDstImage, cDstLayers, cDstOffset,
              cSrcImage, cSrcLayers, cSrcOffset, cExtent);
          } else {
            VkImageResolve region;
            region.srcSubresource = cSrcLayers;
            region.srcOffset      = cSrcOffset;
            region.dstSubresource = cDstLayers;
            region.dstOffset      = cDstOffset;
            region.extent         = cExtent;

            ctx->resolveImage(
              cDstImage, cSrcImage, region, cSrcImage->info().format, VK_RESOLVE_MODE_AVERAGE_BIT,
              VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
          }
        });
      } else {
        DxvkImageViewKey srcViewInfo;
        srcViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        srcViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        srcViewInfo.format = srcImage->info().format;
        srcViewInfo.aspects = srcLayers.aspectMask;
        srcViewInfo.mipIndex = srcLayers.mipLevel;
        srcViewInfo.mipCount = 1u;
        srcViewInfo.layerIndex = srcLayers.baseArrayLayer;
        srcViewInfo.layerCount = 1u;
        srcViewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(SrcTex->GetMapping().Swizzle);

        DxvkImageViewKey dstViewInfo = srcViewInfo;
        dstViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        dstViewInfo.mipIndex = dstLayers.mipLevel;
        dstViewInfo.layerIndex = dstLayers.baseArrayLayer;

        m_device->EmitCs([
          cSrcView  = srcImage->createView(srcViewInfo),
          cDstView  = dstImage->createView(dstViewInfo),
          cDstOffset = DstOffset,
          cSrcOffset = SrcOffset,
          cExtent    = Extent
        ] (DxvkContext* ctx) {
          std::array<VkOffset3D, 4> offsets = {
            cSrcOffset,
            VkOffset3D { cSrcOffset.x + int32_t(cExtent.width),
                         cSrcOffset.y + int32_t(cExtent.width),
                         cSrcOffset.z + int32_t(cExtent.depth) },
            cDstOffset,
            VkOffset3D { cDstOffset.x + int32_t(cExtent.width),
                         cDstOffset.y + int32_t(cExtent.width),
                         cDstOffset.z + int32_t(cExtent.depth) },
          };
          ctx->blitImageView(
            cDstView, &offsets[2],
            cSrcView, offsets.data(),
            VK_FILTER_NEAREST);
        });
      }
  }

  void DxvkLegacyD3DDeviceBridge::CopyRectImageToBuffer(
    const D3D9CommonTexture* SrcTex, D3D9CommonTexture* DstTex,
    uint32_t SrcSubresource, uint32_t DstSubresource,
    VkOffset3D SrcOffset, uint32_t DstOffset, uint32_t DstPitch,
    VkExtent3D Extent, const DxvkFormatInfo* FormatInfo) {
      DstTex->EnsureBufferExists(true);
      m_device->EmitCs([
        cDstBuffer = DstTex->GetBuffer(),
        cSrcImage = SrcTex->GetImage(),
        cDstLayers = vk::makeSubresourceLayers(DstTex->GetSubresourceFromIndex(
          FormatInfo->aspectMask, DstSubresource)),
        cSrcLayers = vk::makeSubresourceLayers(SrcTex->GetSubresourceFromIndex(
          FormatInfo->aspectMask, SrcSubresource)),
        cSrcOffset = SrcOffset,
        cBufferOffset = DstOffset,
        cDstPitch = DstPitch,
        cExtent = Extent
      ] (DxvkContext* ctx) {
        ctx->copyImageToBuffer(cDstBuffer, cBufferOffset, cDstPitch, 0,
          VK_FORMAT_UNDEFINED, cSrcImage, cSrcLayers, cSrcOffset, cExtent);
      });
      m_device->TrackTextureMappingBufferSequenceNumber(DstTex, DstSubresource);
  }

  void DxvkLegacyD3DDeviceBridge::CopyRectBufferToBuffer(
    D3D9CommonTexture* SrcTex, D3D9CommonTexture* DstTex,
    uint32_t SrcSubresource, uint32_t DstSubresource,
    uint32_t SrcOffset, uint32_t DstOffset, uint32_t SrcPitch, uint32_t DstPitch,
    VkExtent3D BlockCount, const DxvkFormatInfo* FormatInfo) {
    VkExtent3D alignedExtent = util::computeBlockExtent(BlockCount, FormatInfo->blockSize);

    const Rc<DxvkBuffer>& srcBuffer = SrcTex->GetBuffer();
    const Rc<DxvkBuffer>& dstBuffer = DstTex->GetBuffer();
    if (unlikely(SrcTex->NeedsReadback(SrcSubresource)
      && !m_device->WaitForResource(*srcBuffer, SrcTex->GetMappingBufferSequenceNumber(SrcSubresource), D3DLOCK_DONOTWAIT | D3DLOCK_READONLY))) {
      Logger::warn("D3D8Device::CopyRects: Stalling because of src buffer");
      m_device->WaitForResource(*srcBuffer, SrcTex->GetMappingBufferSequenceNumber(SrcSubresource), 0);
    }
    if (unlikely(DstTex->NeedsReadback(DstSubresource))
      && !m_device->WaitForResource(*dstBuffer, DstTex->GetMappingBufferSequenceNumber(DstSubresource), D3DLOCK_DONOTWAIT)) {
      Logger::warn("D3D8Device::CopyRects: Stalling because of dst buffer");
      m_device->WaitForResource(*dstBuffer, DstTex->GetMappingBufferSequenceNumber(DstSubresource), 0);
    }
    SrcTex->SetNeedsReadback(SrcSubresource, false);
    DstTex->SetNeedsReadback(DstSubresource, false);

    // We assume that using CopyRects with DEFAULT->SYSTEMMEM and
    // then SYSTEMMEM->SYSTEMMEM doesn't really happen in practice.
    // So the buffers don't need sync and we can just do the copy on the CPU here.

    const void* srcMapPtr = static_cast<const uint8_t*>(SrcTex->GetData(SrcSubresource)) + SrcOffset;
    void* dstMapPtr = static_cast<uint8_t*>(DstTex->GetData(DstSubresource)) + DstOffset;
    VkDeviceSize dirtySize = BlockCount.width * BlockCount.height * BlockCount.depth * FormatInfo->elementSize;
    D3D9BufferSlice slice = m_device->AllocStagingBuffer(dirtySize);
    util::packImageData(
      dstMapPtr, srcMapPtr, SrcPitch, 0, DstPitch, 0, VK_IMAGE_TYPE_2D, alignedExtent, 1, FormatInfo,
      VK_IMAGE_ASPECT_COLOR_BIT);
  }

  bool DxvkLegacyD3DDeviceBridge::IsSupportedSurfaceFormat(D3DFORMAT Format) {
    auto mapping = m_device->LookupFormat(EnumerateFormat(Format));
    return mapping.IsValid();
  }

  DxvkLegacyD3DInterfaceBridge::DxvkLegacyD3DInterfaceBridge(D3D9InterfaceEx* pObject)
    : m_interface(pObject) {
  }

  DxvkLegacyD3DInterfaceBridge::~DxvkLegacyD3DInterfaceBridge() {
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::AddRef() {
    return m_interface->AddRef();
  }

  ULONG STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::Release() {
    return m_interface->Release();
  }

  HRESULT STDMETHODCALLTYPE DxvkLegacyD3DInterfaceBridge::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void DxvkLegacyD3DInterfaceBridge::SetD3DCompatibility(D3DCompatibility d3dCompatibility) const {
    // The D3D9Ex compatibility flag is internal only, and can't be set by the bridge
    if (likely(d3dCompatibility != D3DCompatibility::D3D9Ex)) {
      m_interface->SetD3DCompatibility(d3dCompatibility);
    } else {
      Logger::err("DxvkLegacyD3DInterfaceBridge::SetD3DCompatibility: Invalid compatibility level: D3D9Ex");
    }
  }

  const Config* DxvkLegacyD3DInterfaceBridge::GetConfig() const {
    return &m_interface->GetInstance()->config();
  }

}
