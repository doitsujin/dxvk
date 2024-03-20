#include <algorithm>

#include "d3d11_context.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"

namespace dxvk {

  template<typename ContextType>
  D3D11CommonContext<ContextType>::D3D11CommonContext(
          D3D11Device*            pParent,
    const Rc<DxvkDevice>&         Device,
          UINT                    ContextFlags,
          DxvkCsChunkFlags        CsFlags)
  : D3D11DeviceChild<ID3D11DeviceContext4>(pParent),
    m_contextExt(GetTypedContext()),
    m_annotation(GetTypedContext(), Device),
    m_device    (Device),
    m_flags     (ContextFlags),
    m_staging   (Device, StagingBufferSize),
    m_csFlags   (CsFlags),
    m_csChunk   (AllocCsChunk()),
    m_cmdData   (nullptr) {

  }


  template<typename ContextType>
  D3D11CommonContext<ContextType>::~D3D11CommonContext() {

  }


  template<typename ContextType>
  HRESULT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11DeviceContext)
     || riid == __uuidof(ID3D11DeviceContext1)
     || riid == __uuidof(ID3D11DeviceContext2)
     || riid == __uuidof(ID3D11DeviceContext3)
     || riid == __uuidof(ID3D11DeviceContext4)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11VkExtContext)
     || riid == __uuidof(ID3D11VkExtContext1)) {
      *ppvObject = ref(&m_contextExt);
      return S_OK;
    }

    if (riid == __uuidof(ID3DUserDefinedAnnotation)
     || riid == __uuidof(IDXVKUserDefinedAnnotation)) {
      *ppvObject = ref(&m_annotation);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11DeviceContext), riid)) {
      Logger::warn("D3D11DeviceContext::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  template<typename ContextType>
  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GetType() {
    return IsDeferred
      ? D3D11_DEVICE_CONTEXT_DEFERRED
      : D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }


  template<typename ContextType>
  UINT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GetContextFlags() {
    return m_flags;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearState() {
    D3D10DeviceLock lock = LockContext();

    ResetCommandListState();
    ResetContextState();
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DiscardResource(ID3D11Resource* pResource) {
    D3D10DeviceLock lock = LockContext();

    if (!pResource)
      return;

    // We don't support the Discard API for images
    D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resType);

    if (resType == D3D11_RESOURCE_DIMENSION_BUFFER) {
      DiscardBuffer(pResource);
    } else {
      auto texture = GetCommonTexture(pResource);

      for (uint32_t i = 0; i < texture->CountSubresources(); i++)
        DiscardTexture(pResource, i);
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DiscardView(ID3D11View* pResourceView) {
    DiscardView1(pResourceView, nullptr, 0);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DiscardView1(
          ID3D11View*              pResourceView,
    const D3D11_RECT*              pRects,
          UINT                     NumRects) {
    D3D10DeviceLock lock = LockContext();

    // We don't support discarding individual rectangles
    if (!pResourceView || (NumRects && pRects))
      return;

    // ID3D11View has no methods to query the exact type of
    // the view, so we'll have to check each possible class
    auto dsv = dynamic_cast<D3D11DepthStencilView*>(pResourceView);
    auto rtv = dynamic_cast<D3D11RenderTargetView*>(pResourceView);
    auto uav = dynamic_cast<D3D11UnorderedAccessView*>(pResourceView);

    Rc<DxvkImageView> view;
    if (dsv) view = dsv->GetImageView();
    if (rtv) view = rtv->GetImageView();
    if (uav) view = uav->GetImageView();

    if (view == nullptr)
      return;

    // Get information about underlying resource
    Com<ID3D11Resource> resource;
    pResourceView->GetResource(&resource);

    uint32_t mipCount = GetCommonTexture(resource.ptr())->Desc()->MipLevels;

    // Discard mip levels one by one
    VkImageSubresourceRange sr = view->subresources();

    for (uint32_t layer = 0; layer < sr.layerCount; layer++) {
      for (uint32_t mip = 0; mip < sr.levelCount; mip++) {
        DiscardTexture(resource.ptr(), D3D11CalcSubresource(
          sr.baseMipLevel + mip, sr.baseArrayLayer + layer, mipCount));
      }
    }

    // Since we don't handle SRVs here, we can assume that the
    // view covers all aspects of the underlying resource.
    EmitCs([cView = view] (DxvkContext* ctx) {
      ctx->discardImageView(cView, cView->formatInfo()->aspectMask);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopySubresourceRegion(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox) {
    CopySubresourceRegion1(
      pDstResource, DstSubresource, DstX, DstY, DstZ,
      pSrcResource, SrcSubresource, pSrcBox, 0);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopySubresourceRegion1(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox,
          UINT                              CopyFlags) {
    D3D10DeviceLock lock = LockContext();

    if (!pDstResource || !pSrcResource)
      return;

    if (pSrcBox
     && (pSrcBox->left  >= pSrcBox->right
      || pSrcBox->top   >= pSrcBox->bottom
      || pSrcBox->front >= pSrcBox->back))
      return;

    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;

    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);

    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER && srcResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource);
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource);

      VkDeviceSize dstOffset = DstX;
      VkDeviceSize srcOffset = 0;
      VkDeviceSize byteCount = -1;

      if (pSrcBox) {
        srcOffset = pSrcBox->left;
        byteCount = pSrcBox->right - pSrcBox->left;
      }

      CopyBuffer(dstBuffer, dstOffset, srcBuffer, srcOffset, byteCount);
    } else if (dstResourceDim != D3D11_RESOURCE_DIMENSION_BUFFER && srcResourceDim != D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstTexture = GetCommonTexture(pDstResource);
      auto srcTexture = GetCommonTexture(pSrcResource);

      if (DstSubresource >= dstTexture->CountSubresources()
       || SrcSubresource >= srcTexture->CountSubresources())
        return;

      auto dstFormatInfo = lookupFormatInfo(dstTexture->GetPackedFormat());
      auto srcFormatInfo = lookupFormatInfo(srcTexture->GetPackedFormat());

      auto dstLayers = vk::makeSubresourceLayers(dstTexture->GetSubresourceFromIndex(dstFormatInfo->aspectMask, DstSubresource));
      auto srcLayers = vk::makeSubresourceLayers(srcTexture->GetSubresourceFromIndex(srcFormatInfo->aspectMask, SrcSubresource));

      VkOffset3D srcOffset = { 0, 0, 0 };
      VkOffset3D dstOffset = { int32_t(DstX), int32_t(DstY), int32_t(DstZ) };

      VkExtent3D srcExtent = srcTexture->MipLevelExtent(srcLayers.mipLevel);

      if (pSrcBox) {
        srcOffset.x = pSrcBox->left;
        srcOffset.y = pSrcBox->top;
        srcOffset.z = pSrcBox->front;

        srcExtent.width  = pSrcBox->right -  pSrcBox->left;
        srcExtent.height = pSrcBox->bottom - pSrcBox->top;
        srcExtent.depth  = pSrcBox->back -   pSrcBox->front;
      }

      CopyImage(
        dstTexture, &dstLayers, dstOffset,
        srcTexture, &srcLayers, srcOffset,
        srcExtent);
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    D3D10DeviceLock lock = LockContext();

    if (!pDstResource || !pSrcResource || (pDstResource == pSrcResource))
      return;

    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;

    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);

    if (dstResourceDim != srcResourceDim)
      return;

    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource);
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource);

      if (dstBuffer->Desc()->ByteWidth != srcBuffer->Desc()->ByteWidth)
        return;

      CopyBuffer(dstBuffer, 0, srcBuffer, 0, -1);
    } else {
      auto dstTexture = GetCommonTexture(pDstResource);
      auto srcTexture = GetCommonTexture(pSrcResource);

      auto dstDesc = dstTexture->Desc();
      auto srcDesc = srcTexture->Desc();

      // The subresource count must match as well
      if (dstDesc->ArraySize != srcDesc->ArraySize
       || dstDesc->MipLevels != srcDesc->MipLevels)
        return;

      auto dstFormatInfo = lookupFormatInfo(dstTexture->GetPackedFormat());
      auto srcFormatInfo = lookupFormatInfo(srcTexture->GetPackedFormat());

      for (uint32_t i = 0; i < dstDesc->MipLevels; i++) {
        VkImageSubresourceLayers dstLayers = { dstFormatInfo->aspectMask, i, 0, dstDesc->ArraySize };
        VkImageSubresourceLayers srcLayers = { srcFormatInfo->aspectMask, i, 0, srcDesc->ArraySize };

        CopyImage(
          dstTexture, &dstLayers, VkOffset3D(),
          srcTexture, &srcLayers, VkOffset3D(),
          srcTexture->MipLevelExtent(i));
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopyStructureCount(
          ID3D11Buffer*                     pDstBuffer,
          UINT                              DstAlignedByteOffset,
          ID3D11UnorderedAccessView*        pSrcView) {
    D3D10DeviceLock lock = LockContext();

    auto buf = static_cast<D3D11Buffer*>(pDstBuffer);
    auto uav = static_cast<D3D11UnorderedAccessView*>(pSrcView);

    if (!buf || !uav)
      return;

    auto counterView = uav->GetCounterView();

    if (counterView == nullptr)
      return;

    EmitCs([
      cDstSlice = buf->GetBufferSlice(DstAlignedByteOffset),
      cSrcSlice = counterView->slice()
    ] (DxvkContext* ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset(),
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        sizeof(uint32_t));
    });

    if (buf->HasSequenceNumber())
      GetTypedContext()->TrackBufferSequenceNumber(buf);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearRenderTargetView(
          ID3D11RenderTargetView*           pRenderTargetView,
    const FLOAT                             ColorRGBA[4]) {
    D3D10DeviceLock lock = LockContext();

    auto rtv = static_cast<D3D11RenderTargetView*>(pRenderTargetView);

    if (!rtv)
      return;

    auto view  = rtv->GetImageView();
    auto color = ConvertColorValue(ColorRGBA, view->formatInfo());

    EmitCs([
      cClearValue = color,
      cImageView  = std::move(view)
    ] (DxvkContext* ctx) {
      ctx->clearRenderTarget(
        cImageView,
        VK_IMAGE_ASPECT_COLOR_BIT,
        cClearValue);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearUnorderedAccessViewUint(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const UINT                              Values[4]) {
    D3D10DeviceLock lock = LockContext();

    if (!pUnorderedAccessView)
      return;

    Com<ID3D11UnorderedAccessView> qiUav;

    if (FAILED(pUnorderedAccessView->QueryInterface(IID_PPV_ARGS(&qiUav))))
      return;

    auto uav = static_cast<D3D11UnorderedAccessView*>(qiUav.ptr());

    // Gather UAV format info. We'll use this to determine
    // whether we need to create a temporary view or not.
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    uav->GetDesc(&uavDesc);

    VkFormat uavFormat = m_parent->LookupFormat(uavDesc.Format, DXGI_VK_FORMAT_MODE_ANY).Format;
    VkFormat rawFormat = m_parent->LookupFormat(uavDesc.Format, DXGI_VK_FORMAT_MODE_RAW).Format;

    if (uavFormat != rawFormat && rawFormat == VK_FORMAT_UNDEFINED) {
      Logger::err(str::format("D3D11: ClearUnorderedAccessViewUint: No raw format found for ", uavFormat));
      return;
    }

    VkClearValue clearValue;

    if (uavDesc.Format == DXGI_FORMAT_R11G11B10_FLOAT) {
      // R11G11B10 is a special case since there's no corresponding
      // integer format with the same bit layout. Use R32 instead.
      clearValue.color.uint32[0] = ((Values[0] & 0x7FF) <<  0)
                                 | ((Values[1] & 0x7FF) << 11)
                                 | ((Values[2] & 0x3FF) << 22);
      clearValue.color.uint32[1] = 0;
      clearValue.color.uint32[2] = 0;
      clearValue.color.uint32[3] = 0;
    } else if (uavDesc.Format == DXGI_FORMAT_A8_UNORM) {
      // We need to use R8_UINT to clear A8_UNORM images,
      // so remap the alpha component to the red channel.
      clearValue.color.uint32[0] = Values[3];
      clearValue.color.uint32[1] = 0;
      clearValue.color.uint32[2] = 0;
      clearValue.color.uint32[3] = 0;
    } else {
      clearValue.color.uint32[0] = Values[0];
      clearValue.color.uint32[1] = Values[1];
      clearValue.color.uint32[2] = Values[2];
      clearValue.color.uint32[3] = Values[3];
    }

    if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
      // In case of raw and structured buffers as well as typed
      // buffers that can be used for atomic operations, we can
      // use the fast Vulkan buffer clear function.
      Rc<DxvkBufferView> bufferView = uav->GetBufferView();

      if (bufferView->info().format == VK_FORMAT_R32_UINT
       || bufferView->info().format == VK_FORMAT_R32_SINT
       || bufferView->info().format == VK_FORMAT_R32_SFLOAT
       || bufferView->info().format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
        EmitCs([
          cClearValue = clearValue.color.uint32[0],
          cDstSlice   = bufferView->slice()
        ] (DxvkContext* ctx) {
          ctx->clearBuffer(
            cDstSlice.buffer(),
            cDstSlice.offset(),
            cDstSlice.length(),
            cClearValue);
        });
      } else {
        // Create a view with an integer format if necessary
        if (uavFormat != rawFormat)  {
          DxvkBufferViewCreateInfo info = bufferView->info();
          info.format = rawFormat;

          bufferView = m_device->createBufferView(
            bufferView->buffer(), info);
        }

        EmitCs([
          cClearValue = clearValue,
          cDstView    = bufferView
        ] (DxvkContext* ctx) {
          ctx->clearBufferView(
            cDstView, 0,
            cDstView->elementCount(),
            cClearValue.color);
        });
      }
    } else {
      Rc<DxvkImageView> imageView = uav->GetImageView();

      // If the clear value is zero, we can use the original view regardless of
      // the format since the bit pattern will not change in any supported format.
      bool isZeroClearValue = !(clearValue.color.uint32[0] | clearValue.color.uint32[1]
                              | clearValue.color.uint32[2] | clearValue.color.uint32[3]);

      // Check if we can create an image view with the given raw format. If not,
      // we'll have to use a fallback using a texel buffer view and buffer copies.
      bool isViewCompatible = uavFormat == rawFormat;

      if (!isViewCompatible && (imageView->imageInfo().flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)) {
        uint32_t formatCount = imageView->imageInfo().viewFormatCount;
        isViewCompatible = formatCount == 0;

        for (uint32_t i = 0; i < formatCount && !isViewCompatible; i++)
          isViewCompatible = imageView->imageInfo().viewFormats[i] == rawFormat;
      }

      if (isViewCompatible || isZeroClearValue) {
        // Create a view with an integer format if necessary
        if (uavFormat != rawFormat && !isZeroClearValue) {
          DxvkImageViewCreateInfo info = imageView->info();
          info.format = rawFormat;

          imageView = m_device->createImageView(imageView->image(), info);
        }

        EmitCs([
          cClearValue = clearValue,
          cDstView    = imageView
        ] (DxvkContext* ctx) {
          ctx->clearImageView(cDstView,
            VkOffset3D { 0, 0, 0 },
            cDstView->mipLevelExtent(0),
            VK_IMAGE_ASPECT_COLOR_BIT,
            cClearValue);
        });
      } else {
        DxvkBufferCreateInfo bufferInfo;
        bufferInfo.size   = imageView->formatInfo()->elementSize
                          * imageView->info().numLayers
                          * util::flattenImageExtent(imageView->mipLevelExtent(0));
        bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                          | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                          | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        bufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT
                          | VK_ACCESS_SHADER_WRITE_BIT;

        Rc<DxvkBuffer> buffer = m_device->createBuffer(bufferInfo,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        DxvkBufferViewCreateInfo bufferViewInfo;
        bufferViewInfo.format      = rawFormat;
        bufferViewInfo.rangeOffset = 0;
        bufferViewInfo.rangeLength = bufferInfo.size;

        Rc<DxvkBufferView> bufferView = m_device->createBufferView(buffer,
          bufferViewInfo);

        EmitCs([
          cDstView    = std::move(imageView),
          cSrcView    = std::move(bufferView),
          cClearValue = clearValue.color
        ] (DxvkContext* ctx) {
          ctx->clearBufferView(
            cSrcView, 0,
            cSrcView->elementCount(),
            cClearValue);

          ctx->copyBufferToImage(cDstView->image(),
            vk::pickSubresourceLayers(cDstView->subresources(), 0),
            VkOffset3D { 0, 0, 0 },
            cDstView->mipLevelExtent(0),
            cSrcView->buffer(), 0, 0, 0);
        });
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearUnorderedAccessViewFloat(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const FLOAT                             Values[4]) {
    D3D10DeviceLock lock = LockContext();

    auto uav = static_cast<D3D11UnorderedAccessView*>(pUnorderedAccessView);

    if (!uav)
      return;

    auto imgView = uav->GetImageView();
    auto bufView = uav->GetBufferView();

    const DxvkFormatInfo* info = nullptr;
    if (imgView != nullptr) info = imgView->formatInfo();
    if (bufView != nullptr) info = bufView->formatInfo();

    if (!info || info->flags.any(DxvkFormatFlag::SampledSInt, DxvkFormatFlag::SampledUInt))
      return;

    VkClearValue clearValue;
    clearValue.color.float32[0] = Values[0];
    clearValue.color.float32[1] = Values[1];
    clearValue.color.float32[2] = Values[2];
    clearValue.color.float32[3] = Values[3];

    if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
      EmitCs([
        cClearValue = clearValue,
        cDstView    = std::move(bufView)
      ] (DxvkContext* ctx) {
        ctx->clearBufferView(
          cDstView, 0,
          cDstView->elementCount(),
          cClearValue.color);
      });
    } else {
      EmitCs([
        cClearValue = clearValue,
        cDstView    = std::move(imgView)
      ] (DxvkContext* ctx) {
        ctx->clearImageView(cDstView,
          VkOffset3D { 0, 0, 0 },
          cDstView->mipLevelExtent(0),
          VK_IMAGE_ASPECT_COLOR_BIT,
          cClearValue);
      });
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearDepthStencilView(
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    D3D10DeviceLock lock = LockContext();

    auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);

    if (!dsv)
      return;

    // Figure out which aspects to clear based on
    // the image view properties and clear flags.
    VkImageAspectFlags aspectMask = 0;

    if (ClearFlags & D3D11_CLEAR_DEPTH)
      aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

    if (ClearFlags & D3D11_CLEAR_STENCIL)
      aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    aspectMask &= dsv->GetWritableAspectMask();

    if (!aspectMask)
      return;

    VkClearValue clearValue;
    clearValue.depthStencil.depth   = Depth;
    clearValue.depthStencil.stencil = Stencil;

    EmitCs([
      cClearValue = clearValue,
      cAspectMask = aspectMask,
      cImageView  = dsv->GetImageView()
    ] (DxvkContext* ctx) {
      ctx->clearRenderTarget(
        cImageView,
        cAspectMask,
        cClearValue);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ClearView(
          ID3D11View*                       pView,
    const FLOAT                             Color[4],
    const D3D11_RECT*                       pRect,
          UINT                              NumRects) {
    D3D10DeviceLock lock = LockContext();

    if (NumRects && !pRect)
      return;

    // ID3D11View has no methods to query the exact type of
    // the view, so we'll have to check each possible class
    auto dsv = dynamic_cast<D3D11DepthStencilView*>(pView);
    auto rtv = dynamic_cast<D3D11RenderTargetView*>(pView);
    auto uav = dynamic_cast<D3D11UnorderedAccessView*>(pView);
    auto vov = dynamic_cast<D3D11VideoProcessorOutputView*>(pView);

    // Retrieve underlying resource view
    Rc<DxvkBufferView> bufView;
    Rc<DxvkImageView>  imgView;

    if (dsv != nullptr)
      imgView = dsv->GetImageView();

    if (rtv != nullptr)
      imgView = rtv->GetImageView();

    if (uav != nullptr) {
      bufView = uav->GetBufferView();
      imgView = uav->GetImageView();
    }

    if (vov != nullptr)
      imgView = vov->GetView();

    // 3D views are unsupported
    if (imgView != nullptr
     && imgView->info().type == VK_IMAGE_VIEW_TYPE_3D)
      return;

    // Query the view format. We'll have to convert
    // the clear color based on the format's data type.
    VkFormat format = VK_FORMAT_UNDEFINED;

    if (bufView != nullptr)
      format = bufView->info().format;

    if (imgView != nullptr)
      format = imgView->info().format;

    if (format == VK_FORMAT_UNDEFINED)
      return;

    // We'll need the format info to determine the buffer
    // element size, and we also need it for depth images.
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(format);

    // Convert the clear color format. ClearView takes
    // the clear value for integer formats as a set of
    // integral floats, so we'll have to convert.
    VkClearValue        clearValue  = ConvertColorValue(Color, formatInfo);
    VkImageAspectFlags  clearAspect = formatInfo->aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);

    // Clear all the rectangles that are specified
    for (uint32_t i = 0; i < NumRects || i < 1; i++) {
      if (pRect) {
        if (pRect[i].left >= pRect[i].right
        || pRect[i].top >= pRect[i].bottom)
          continue;
      }

      if (bufView != nullptr) {
        VkDeviceSize offset = 0;
        VkDeviceSize length = bufView->info().rangeLength / formatInfo->elementSize;

        if (pRect) {
          offset = pRect[i].left;
          length = pRect[i].right - pRect[i].left;
        }

        EmitCs([
          cBufferView   = bufView,
          cRangeOffset  = offset,
          cRangeLength  = length,
          cClearValue   = clearValue
        ] (DxvkContext* ctx) {
          ctx->clearBufferView(
            cBufferView,
            cRangeOffset,
            cRangeLength,
            cClearValue.color);
        });
      }

      if (imgView != nullptr) {
        VkOffset3D offset = { 0, 0, 0 };
        VkExtent3D extent = imgView->mipLevelExtent(0);

        if (pRect) {
          offset = { pRect[i].left, pRect[i].top, 0 };
          extent = {
            uint32_t(pRect[i].right - pRect[i].left),
            uint32_t(pRect[i].bottom - pRect[i].top), 1 };
        }

        EmitCs([
          cImageView    = imgView,
          cAreaOffset   = offset,
          cAreaExtent   = extent,
          cClearAspect  = clearAspect,
          cClearValue   = clearValue
        ] (DxvkContext* ctx) {
          const VkImageUsageFlags rtUsage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

          bool isFullSize = cImageView->mipLevelExtent(0) == cAreaExtent;

          if ((cImageView->info().usage & rtUsage) && isFullSize) {
            ctx->clearRenderTarget(
              cImageView,
              cClearAspect,
              cClearValue);
          } else {
            ctx->clearImageView(
              cImageView,
              cAreaOffset,
              cAreaExtent,
              cClearAspect,
              cClearValue);
          }
        });
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) {
    D3D10DeviceLock lock = LockContext();

    auto view = static_cast<D3D11ShaderResourceView*>(pShaderResourceView);

    if (!view || view->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER)
      return;

    D3D11_COMMON_RESOURCE_DESC resourceDesc = view->GetResourceDesc();

    if (!(resourceDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS))
      return;

    EmitCs([cDstImageView = view->GetImageView()]
    (DxvkContext* ctx) {
      ctx->generateMipmaps(cDstImageView, VK_FILTER_LINEAR);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ResolveSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    D3D10DeviceLock lock = LockContext();

    bool isSameSubresource = pDstResource   == pSrcResource
                          && DstSubresource == SrcSubresource;

    if (!pDstResource || !pSrcResource || isSameSubresource)
      return;

    D3D11_RESOURCE_DIMENSION dstResourceType;
    D3D11_RESOURCE_DIMENSION srcResourceType;

    pDstResource->GetType(&dstResourceType);
    pSrcResource->GetType(&srcResourceType);

    if (dstResourceType != D3D11_RESOURCE_DIMENSION_TEXTURE2D
     || srcResourceType != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      return;

    auto dstTexture = static_cast<D3D11Texture2D*>(pDstResource);
    auto srcTexture = static_cast<D3D11Texture2D*>(pSrcResource);

    D3D11_TEXTURE2D_DESC dstDesc;
    D3D11_TEXTURE2D_DESC srcDesc;

    dstTexture->GetDesc(&dstDesc);
    srcTexture->GetDesc(&srcDesc);

    if (dstDesc.SampleDesc.Count != 1)
      return;

    D3D11CommonTexture* dstTextureInfo = GetCommonTexture(pDstResource);
    D3D11CommonTexture* srcTextureInfo = GetCommonTexture(pSrcResource);

    const DXGI_VK_FORMAT_INFO dstFormatInfo = m_parent->LookupFormat(dstDesc.Format, DXGI_VK_FORMAT_MODE_ANY);
    const DXGI_VK_FORMAT_INFO srcFormatInfo = m_parent->LookupFormat(srcDesc.Format, DXGI_VK_FORMAT_MODE_ANY);

    auto dstVulkanFormatInfo = lookupFormatInfo(dstFormatInfo.Format);
    auto srcVulkanFormatInfo = lookupFormatInfo(srcFormatInfo.Format);

    if (DstSubresource >= dstTextureInfo->CountSubresources()
     || SrcSubresource >= srcTextureInfo->CountSubresources())
      return;

    const VkImageSubresource dstSubresource =
      dstTextureInfo->GetSubresourceFromIndex(
        dstVulkanFormatInfo->aspectMask, DstSubresource);

    const VkImageSubresource srcSubresource =
      srcTextureInfo->GetSubresourceFromIndex(
        srcVulkanFormatInfo->aspectMask, SrcSubresource);

    const VkImageSubresourceLayers dstSubresourceLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };

    const VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };

    if (srcDesc.SampleDesc.Count == 1 || m_parent->GetOptions()->disableMsaa) {
      EmitCs([
        cDstImage  = dstTextureInfo->GetImage(),
        cSrcImage  = srcTextureInfo->GetImage(),
        cDstLayers = dstSubresourceLayers,
        cSrcLayers = srcSubresourceLayers
      ] (DxvkContext* ctx) {
        ctx->copyImage(
          cDstImage, cDstLayers, VkOffset3D { 0, 0, 0 },
          cSrcImage, cSrcLayers, VkOffset3D { 0, 0, 0 },
          cDstImage->mipLevelExtent(cDstLayers.mipLevel));
      });
    } else {
      const VkFormat format = m_parent->LookupFormat(
        Format, DXGI_VK_FORMAT_MODE_ANY).Format;

      EmitCs([
        cDstImage  = dstTextureInfo->GetImage(),
        cSrcImage  = srcTextureInfo->GetImage(),
        cDstSubres = dstSubresourceLayers,
        cSrcSubres = srcSubresourceLayers,
        cFormat    = format
      ] (DxvkContext* ctx) {
        VkImageResolve region;
        region.srcSubresource = cSrcSubres;
        region.srcOffset      = VkOffset3D { 0, 0, 0 };
        region.dstSubresource = cDstSubres;
        region.dstOffset      = VkOffset3D { 0, 0, 0 };
        region.extent         = cDstImage->mipLevelExtent(cDstSubres.mipLevel);

        ctx->resolveImage(cDstImage, cSrcImage, region, cFormat);
      });
    }

    if (dstTextureInfo->HasSequenceNumber())
      GetTypedContext()->TrackTextureSequenceNumber(dstTextureInfo, DstSubresource);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    if (IsDeferred && unlikely(pDstBox != nullptr) && unlikely(!m_parent->GetOptions()->exposeDriverCommandLists)) {
      // If called from a deferred context and native command list support is not
      // exposed, we need to apply the destination box to the source pointer. This
      // only applies to UpdateSubresource, not to UpdateSubresource1. See MSDN:
      // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476486(v=vs.85).aspx)
      size_t srcOffset = pDstBox->left;

      // For textures, the offset logic needs to take the format into account.
      // Ignore that multi-planar images exist, this is hairy enough already.
      D3D11CommonTexture* dstTexture = GetCommonTexture(pDstResource);

      if (dstTexture) {
        auto dstFormat = dstTexture->GetPackedFormat();
        auto dstFormatInfo = lookupFormatInfo(dstFormat);

        size_t blockSize = dstFormatInfo->elementSize;

        VkOffset3D offset;
        offset.x = pDstBox->left / dstFormatInfo->blockSize.width;
        offset.y = pDstBox->top / dstFormatInfo->blockSize.height;
        offset.z = pDstBox->front / dstFormatInfo->blockSize.depth;

        srcOffset = offset.x * blockSize + offset.y * SrcRowPitch + offset.z * SrcDepthPitch;
      }

      pSrcData = reinterpret_cast<const char*>(pSrcData) + srcOffset;
    }

    UpdateResource(pDstResource, DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch, 0);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::UpdateSubresource1(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch,
          UINT                              CopyFlags) {
    UpdateResource(pDstResource, DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawAuto() {
    D3D10DeviceLock lock = LockContext();

    D3D11Buffer* buffer = m_state.ia.vertexBuffers[0].buffer.ptr();

    if (!buffer)
      return;

    DxvkBufferSlice vtxBuf = buffer->GetBufferSlice();
    DxvkBufferSlice ctrBuf = buffer->GetSOCounter();

    if (!ctrBuf.defined())
      return;

    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndirectXfb(ctrBuf,
        vtxBuf.buffer()->getXfbVertexStride(),
        vtxBuf.offset());
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::Draw(
          UINT            VertexCount,
          UINT            StartVertexLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCount, 1,
        StartVertexLocation, 0);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawIndexed(
          UINT            IndexCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCount, 1,
        StartIndexLocation,
        BaseVertexLocation, 0);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawInstanced(
          UINT            VertexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartVertexLocation,
          UINT            StartInstanceLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCountPerInstance,
        InstanceCount,
        StartVertexLocation,
        StartInstanceLocation);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawIndexedInstanced(
          UINT            IndexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation,
          UINT            StartInstanceLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCountPerInstance,
        InstanceCount,
        StartIndexLocation,
        BaseVertexLocation,
        StartInstanceLocation);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawIndexedInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);

    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDrawIndexedIndirectCommand)))
      return;

    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);
    auto stride = 0u;

    if (cmdData && cmdData->type == D3D11CmdType::DrawIndirectIndexed)
      stride = GetIndirectCommandStride(cmdData, AlignedByteOffsetForArgs, sizeof(VkDrawIndexedIndirectCommand));

    if (stride) {
      cmdData->count += 1;
      cmdData->stride = stride;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndexedIndirect(data->offset, data->count, data->stride);
        });

      cmdData->type   = D3D11CmdType::DrawIndirectIndexed;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
      cmdData->stride = 0;
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DrawInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);

    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDrawIndirectCommand)))
      return;

    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);
    auto stride = 0u;

    if (cmdData && cmdData->type == D3D11CmdType::DrawIndirect)
      stride = GetIndirectCommandStride(cmdData, AlignedByteOffsetForArgs, sizeof(VkDrawIndirectCommand));

    if (stride) {
      cmdData->count += 1;
      cmdData->stride = stride;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndirect(data->offset, data->count, data->stride);
        });

      cmdData->type   = D3D11CmdType::DrawIndirect;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
      cmdData->stride = 0;
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::Dispatch(
          UINT            ThreadGroupCountX,
          UINT            ThreadGroupCountY,
          UINT            ThreadGroupCountZ) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->dispatch(
        ThreadGroupCountX,
        ThreadGroupCountY,
        ThreadGroupCountZ);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DispatchIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);

    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDispatchIndirectCommand)))
      return;

    EmitCs([cOffset = AlignedByteOffsetForArgs]
    (DxvkContext* ctx) {
      ctx->dispatchIndirect(cOffset);
    });
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
    D3D10DeviceLock lock = LockContext();

    auto inputLayout = static_cast<D3D11InputLayout*>(pInputLayout);

    if (m_state.ia.inputLayout != inputLayout) {
      bool equal = false;

      // Some games (e.g. Grim Dawn) create lots and lots of
      // identical input layouts, so we'll only apply the state
      // if the input layouts has actually changed between calls.
      if (m_state.ia.inputLayout != nullptr && inputLayout != nullptr)
        equal = m_state.ia.inputLayout->Compare(inputLayout);

      m_state.ia.inputLayout = inputLayout;

      if (!equal)
        ApplyInputLayout();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    D3D10DeviceLock lock = LockContext();

    if (m_state.ia.primitiveTopology != Topology) {
      m_state.ia.primitiveTopology = Topology;
      ApplyPrimitiveTopology();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppVertexBuffers[i]);

      if (m_state.ia.vertexBuffers[StartSlot + i].buffer != newBuffer) {
        m_state.ia.vertexBuffers[StartSlot + i].buffer = newBuffer;
        m_state.ia.vertexBuffers[StartSlot + i].offset = pOffsets[i];
        m_state.ia.vertexBuffers[StartSlot + i].stride = pStrides[i];

        BindVertexBuffer(StartSlot + i, newBuffer, pOffsets[i], pStrides[i]);
      } else if (m_state.ia.vertexBuffers[StartSlot + i].offset != pOffsets[i]
              || m_state.ia.vertexBuffers[StartSlot + i].stride != pStrides[i]) {
        m_state.ia.vertexBuffers[StartSlot + i].offset = pOffsets[i];
        m_state.ia.vertexBuffers[StartSlot + i].stride = pStrides[i];

        BindVertexBufferRange(StartSlot + i, newBuffer, pOffsets[i], pStrides[i]);
      }
    }

    m_state.ia.maxVbCount = std::clamp(StartSlot + NumBuffers,
      m_state.ia.maxVbCount, uint32_t(m_state.ia.vertexBuffers.size()));
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    D3D10DeviceLock lock = LockContext();

    auto newBuffer = static_cast<D3D11Buffer*>(pIndexBuffer);

    if (m_state.ia.indexBuffer.buffer != newBuffer) {
      m_state.ia.indexBuffer.buffer = newBuffer;
      m_state.ia.indexBuffer.offset = Offset;
      m_state.ia.indexBuffer.format = Format;

      BindIndexBuffer(newBuffer, Offset, Format);
    } else if (m_state.ia.indexBuffer.offset != Offset
            || m_state.ia.indexBuffer.format != Format) {
      m_state.ia.indexBuffer.offset = Offset;
      m_state.ia.indexBuffer.format = Format;

      BindIndexBufferRange(newBuffer, Offset, Format);
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) {
    D3D10DeviceLock lock = LockContext();

    *ppInputLayout = m_state.ia.inputLayout.ref();
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) {
    D3D10DeviceLock lock = LockContext();

    *pTopology = m_state.ia.primitiveTopology;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IAGetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppVertexBuffers,
          UINT*                             pStrides,
          UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumBuffers; i++) {
      const bool inRange = StartSlot + i < m_state.ia.vertexBuffers.size();

      if (ppVertexBuffers) {
        ppVertexBuffers[i] = inRange
          ? m_state.ia.vertexBuffers[StartSlot + i].buffer.ref()
          : nullptr;
      }

      if (pStrides) {
        pStrides[i] = inRange
          ? m_state.ia.vertexBuffers[StartSlot + i].stride
          : 0u;
      }

      if (pOffsets) {
        pOffsets[i] = inRange
          ? m_state.ia.vertexBuffers[StartSlot + i].offset
          : 0u;
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IAGetIndexBuffer(
          ID3D11Buffer**                    ppIndexBuffer,
          DXGI_FORMAT*                      pFormat,
          UINT*                             pOffset) {
    D3D10DeviceLock lock = LockContext();

    if (ppIndexBuffer)
      *ppIndexBuffer = m_state.ia.indexBuffer.buffer.ref();

    if (pFormat)
      *pFormat = m_state.ia.indexBuffer.format;

    if (pOffset)
      *pOffset = m_state.ia.indexBuffer.offset;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSSetShader(
          ID3D11VertexShader*               pVertexShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11VertexShader*>(pVertexShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.vs != shader) {
      m_state.vs = shader;

      BindShader<DxbcProgramType::VertexShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::VertexShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::VertexShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::VertexShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::VertexShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetShader(
          ID3D11VertexShader**              ppVertexShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppVertexShader)
      *ppVertexShader = m_state.vs.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::VertexShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::VertexShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::VertexShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::VertexShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSSetShader(
          ID3D11HullShader*                 pHullShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11HullShader*>(pHullShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.hs != shader) {
      m_state.hs = shader;

      BindShader<DxbcProgramType::HullShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::HullShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::HullShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::HullShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::HullShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetShader(
          ID3D11HullShader**                ppHullShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppHullShader)
      *ppHullShader = m_state.hs.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::HullShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::HullShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::HullShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::HullShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSSetShader(
          ID3D11DomainShader*               pDomainShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11DomainShader*>(pDomainShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.ds != shader) {
      m_state.ds = shader;

      BindShader<DxbcProgramType::DomainShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::DomainShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::DomainShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::DomainShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::DomainShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetShader(
          ID3D11DomainShader**              ppDomainShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppDomainShader)
      *ppDomainShader = m_state.ds.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::DomainShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::DomainShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::DomainShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::DomainShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSSetShader(
          ID3D11GeometryShader*             pShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11GeometryShader*>(pShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.gs != shader) {
      m_state.gs = shader;

      BindShader<DxbcProgramType::GeometryShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::GeometryShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {  
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::GeometryShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::GeometryShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::GeometryShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetShader(
          ID3D11GeometryShader**            ppGeometryShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppGeometryShader)
      *ppGeometryShader = m_state.gs.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::GeometryShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::GeometryShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::GeometryShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::GeometryShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSSetShader(
          ID3D11PixelShader*                pPixelShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11PixelShader*>(pPixelShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.ps != shader) {
      m_state.ps = shader;

      BindShader<DxbcProgramType::PixelShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::PixelShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::PixelShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::PixelShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::PixelShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetShader(
          ID3D11PixelShader**               ppPixelShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppPixelShader)
      *ppPixelShader = m_state.ps.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::PixelShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::PixelShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::PixelShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::PixelShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetShader(
          ID3D11ComputeShader*              pComputeShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    auto shader = static_cast<D3D11ComputeShader*>(pComputeShader);

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    if (m_state.cs != shader) {
      m_state.cs = shader;

      BindShader<DxbcProgramType::ComputeShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers<DxbcProgramType::ComputeShader>(
      StartSlot, NumBuffers, ppConstantBuffers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    SetConstantBuffers1<DxbcProgramType::ComputeShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    SetShaderResources<DxbcProgramType::ComputeShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    SetSamplers<DxbcProgramType::ComputeShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSSetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    D3D10DeviceLock lock = LockContext();

    if (TestRtvUavHazards(0, nullptr, NumUAVs, ppUnorderedAccessViews))
      return;

    // Unbind previously bound conflicting UAVs
    uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::ComputeShader, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::ComputeShader, 0);

    int32_t uavId = m_state.uav.mask.findNext(0);

    while (uavId >= 0) {
      if (uint32_t(uavId) < StartSlot || uint32_t(uavId) >= StartSlot + NumUAVs) {
        for (uint32_t i = 0; i < NumUAVs; i++) {
          auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);

          if (CheckViewOverlap(uav, m_state.uav.views[uavId].ptr())) {
            m_state.uav.views[uavId] = nullptr;
            m_state.uav.mask.clr(uavId);

            BindUnorderedAccessView<DxbcProgramType::ComputeShader>(
              uavSlotId + uavId, nullptr,
              ctrSlotId + uavId, ~0u);
          }
        }

        uavId = m_state.uav.mask.findNext(uavId + 1);
      } else {
        uavId = m_state.uav.mask.findNext(StartSlot + NumUAVs);
      }
    }

    // Actually bind the given UAVs
    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);
      auto ctr = pUAVInitialCounts ? pUAVInitialCounts[i] : ~0u;

      if (m_state.uav.views[StartSlot + i] != uav || ctr != ~0u) {
        m_state.uav.views[StartSlot + i] = uav;
        m_state.uav.mask.set(StartSlot + i, uav != nullptr);

        BindUnorderedAccessView<DxbcProgramType::ComputeShader>(
          uavSlotId + StartSlot + i, uav,
          ctrSlotId + StartSlot + i, ctr);

        ResolveCsSrvHazards(uav);
      }
    }

    m_state.uav.maxCount = std::clamp(StartSlot + NumUAVs,
      m_state.uav.maxCount, uint32_t(m_state.uav.views.size()));
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetShader(
          ID3D11ComputeShader**             ppComputeShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppComputeShader)
      *ppComputeShader = m_state.cs.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::ComputeShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();

    GetConstantBuffers<DxbcProgramType::ComputeShader>(
      StartSlot, NumBuffers, ppConstantBuffers,
      pFirstConstant, pNumConstants);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();

    GetShaderResources<DxbcProgramType::ComputeShader>(
      StartSlot, NumViews, ppShaderResourceViews);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();

    GetSamplers<DxbcProgramType::ComputeShader>(
      StartSlot, NumSamplers, ppSamplers);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumUAVs; i++) {
      ppUnorderedAccessViews[i] = StartSlot + i < m_state.uav.views.size()
        ? m_state.uav.views[StartSlot + i].ref()
        : nullptr;
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    D3D10DeviceLock lock = LockContext();

    SetRenderTargetsAndUnorderedAccessViews(
      NumViews, ppRenderTargetViews, pDepthStencilView,
      NumViews, 0, nullptr, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    D3D10DeviceLock lock = LockContext();

    SetRenderTargetsAndUnorderedAccessViews(
      NumRTVs, ppRenderTargetViews, pDepthStencilView,
      UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMSetBlendState(
          ID3D11BlendState*                 pBlendState,
    const FLOAT                             BlendFactor[4],
          UINT                              SampleMask) {
    D3D10DeviceLock lock = LockContext();

    auto blendState = static_cast<D3D11BlendState*>(pBlendState);

    if (m_state.om.cbState    != blendState
     || m_state.om.sampleMask != SampleMask) {
      m_state.om.cbState    = blendState;
      m_state.om.sampleMask = SampleMask;

      ApplyBlendState();
    }

    if (BlendFactor != nullptr) {
      for (uint32_t i = 0; i < 4; i++)
        m_state.om.blendFactor[i] = BlendFactor[i];

      ApplyBlendFactor();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMSetDepthStencilState(
          ID3D11DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    D3D10DeviceLock lock = LockContext();

    auto depthStencilState = static_cast<D3D11DepthStencilState*>(pDepthStencilState);

    if (m_state.om.dsState != depthStencilState) {
      m_state.om.dsState = depthStencilState;
      ApplyDepthStencilState();
    }

    // The D3D11 runtime only appears to store the low 8 bits,
    // and some games rely on this behaviour. Do the same here.
    StencilRef &= 0xFF;

    if (m_state.om.stencilRef != StencilRef) {
      m_state.om.stencilRef = StencilRef;
      ApplyStencilRef();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    OMGetRenderTargetsAndUnorderedAccessViews(
      NumViews, ppRenderTargetViews, ppDepthStencilView,
      NumViews, 0, nullptr);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMGetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    D3D10DeviceLock lock = LockContext();

    if (ppRenderTargetViews) {
      for (UINT i = 0; i < NumRTVs; i++) {
        ppRenderTargetViews[i] = i < m_state.om.rtvs.size()
          ? m_state.om.rtvs[i].ref()
          : nullptr;
      }
    }

    if (ppDepthStencilView)
      *ppDepthStencilView = m_state.om.dsv.ref();

    if (ppUnorderedAccessViews) {
      for (UINT i = 0; i < NumUAVs; i++) {
        ppUnorderedAccessViews[i] = UAVStartSlot + i < m_state.om.uavs.size()
          ? m_state.om.uavs[UAVStartSlot + i].ref()
          : nullptr;
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMGetBlendState(
          ID3D11BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    D3D10DeviceLock lock = LockContext();

    if (ppBlendState)
      *ppBlendState = ref(m_state.om.cbState);

    if (BlendFactor)
      std::memcpy(BlendFactor, m_state.om.blendFactor, sizeof(FLOAT) * 4);

    if (pSampleMask)
      *pSampleMask = m_state.om.sampleMask;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMGetDepthStencilState(
          ID3D11DepthStencilState**         ppDepthStencilState,
          UINT*                             pStencilRef) {
    D3D10DeviceLock lock = LockContext();

    if (ppDepthStencilState)
      *ppDepthStencilState = ref(m_state.om.dsState);

    if (pStencilRef)
      *pStencilRef = m_state.om.stencilRef;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    D3D10DeviceLock lock = LockContext();

    auto currRasterizerState = m_state.rs.state;
    auto nextRasterizerState = static_cast<D3D11RasterizerState*>(pRasterizerState);

    if (m_state.rs.state != nextRasterizerState) {
      m_state.rs.state = nextRasterizerState;
      ApplyRasterizerState();

      // If necessary, update the rasterizer sample count push constant
      uint32_t currSampleCount = currRasterizerState != nullptr ? currRasterizerState->Desc()->ForcedSampleCount : 0;
      uint32_t nextSampleCount = nextRasterizerState != nullptr ? nextRasterizerState->Desc()->ForcedSampleCount : 0;

      if (currSampleCount != nextSampleCount)
        ApplyRasterizerSampleCount();

      // In D3D11, the rasterizer state defines whether the scissor test is
      // enabled, so if that changes, we need to update scissor rects as well.
      bool currScissorEnable = currRasterizerState != nullptr ? currRasterizerState->Desc()->ScissorEnable : false;
      bool nextScissorEnable = nextRasterizerState != nullptr ? nextRasterizerState->Desc()->ScissorEnable : false;

      if (currScissorEnable != nextScissorEnable)
        ApplyViewportState();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSSetViewports(
          UINT                              NumViewports,
    const D3D11_VIEWPORT*                   pViewports) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(NumViewports > m_state.rs.viewports.size()))
      return;

    bool dirty = m_state.rs.numViewports != NumViewports;
    m_state.rs.numViewports = NumViewports;

    for (uint32_t i = 0; i < NumViewports; i++) {
      const D3D11_VIEWPORT& vp = m_state.rs.viewports[i];

      dirty |= vp.TopLeftX != pViewports[i].TopLeftX
            || vp.TopLeftY != pViewports[i].TopLeftY
            || vp.Width    != pViewports[i].Width
            || vp.Height   != pViewports[i].Height
            || vp.MinDepth != pViewports[i].MinDepth
            || vp.MaxDepth != pViewports[i].MaxDepth;
      
      m_state.rs.viewports[i] = pViewports[i];
    }

    if (dirty)
      ApplyViewportState();
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSSetScissorRects(
          UINT                              NumRects,
    const D3D11_RECT*                       pRects) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(NumRects > m_state.rs.scissors.size()))
      return;

    bool dirty = m_state.rs.numScissors != NumRects;
    m_state.rs.numScissors = NumRects;

    for (uint32_t i = 0; i < NumRects; i++) {
      if (pRects[i].bottom >= pRects[i].top
       && pRects[i].right  >= pRects[i].left) {
        const D3D11_RECT& sr = m_state.rs.scissors[i];

        dirty |= sr.top    != pRects[i].top
              || sr.left   != pRects[i].left
              || sr.bottom != pRects[i].bottom
              || sr.right  != pRects[i].right;

        m_state.rs.scissors[i] = pRects[i];
      }
    }

    if (m_state.rs.state != nullptr && dirty) {
      D3D11_RASTERIZER_DESC rsDesc;
      m_state.rs.state->GetDesc(&rsDesc);

      if (rsDesc.ScissorEnable)
        ApplyViewportState();
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    D3D10DeviceLock lock = LockContext();

    if (ppRasterizerState)
      *ppRasterizerState = ref(m_state.rs.state);
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    D3D10DeviceLock lock = LockContext();
    uint32_t numWritten = m_state.rs.numViewports;

    if (pViewports) {
      numWritten = std::min(numWritten, *pNumViewports);

      for (uint32_t i = 0; i < *pNumViewports; i++) {
        if (i < m_state.rs.numViewports) {
          pViewports[i] = m_state.rs.viewports[i];
        } else {
          pViewports[i].TopLeftX = 0.0f;
          pViewports[i].TopLeftY = 0.0f;
          pViewports[i].Width    = 0.0f;
          pViewports[i].Height   = 0.0f;
          pViewports[i].MinDepth = 0.0f;
          pViewports[i].MaxDepth = 0.0f;
        }
      }
    }

    *pNumViewports = numWritten;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::RSGetScissorRects(
          UINT*                             pNumRects,
          D3D11_RECT*                       pRects) {
    D3D10DeviceLock lock = LockContext();
    uint32_t numWritten = m_state.rs.numScissors;

    if (pRects) {
      numWritten = std::min(numWritten, *pNumRects);

      for (uint32_t i = 0; i < *pNumRects; i++) {
        if (i < m_state.rs.numScissors) {
          pRects[i] = m_state.rs.scissors[i];
        } else {
          pRects[i].left   = 0;
          pRects[i].top    = 0;
          pRects[i].right  = 0;
          pRects[i].bottom = 0;
        }
      }
    }

    *pNumRects = m_state.rs.numScissors;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SOSetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppSOTargets,
    const UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumBuffers; i++) {
      D3D11Buffer* buffer = static_cast<D3D11Buffer*>(ppSOTargets[i]);
      UINT         offset = pOffsets != nullptr ? pOffsets[i] : 0;

      m_state.so.targets[i].buffer = buffer;
      m_state.so.targets[i].offset = offset;
    }

    for (uint32_t i = NumBuffers; i < D3D11_SO_BUFFER_SLOT_COUNT; i++) {
      m_state.so.targets[i].buffer = nullptr;
      m_state.so.targets[i].offset = 0;
    }

    for (uint32_t i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++) {
      BindXfbBuffer(i,
        m_state.so.targets[i].buffer.ptr(),
        m_state.so.targets[i].offset);
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SOGetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumBuffers; i++) {
      ppSOTargets[i] = i < m_state.so.targets.size()
        ? m_state.so.targets[i].buffer.ref()
        : nullptr;
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SOGetTargetsWithOffsets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets,
          UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();

    for (uint32_t i = 0; i < NumBuffers; i++) {
      const bool inRange = i < m_state.so.targets.size();

      if (ppSOTargets) {
        ppSOTargets[i] = inRange
          ? m_state.so.targets[i].buffer.ref()
          : nullptr;
      }

      if (pOffsets) {
        pOffsets[i] = inRange
          ? m_state.so.targets[i].offset
          : 0u;
      }
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SetPredication(
          ID3D11Predicate*                  pPredicate,
          BOOL                              PredicateValue) {
    D3D10DeviceLock lock = LockContext();

    auto predicate = D3D11Query::FromPredicate(pPredicate);
    m_state.pr.predicateObject = predicate;
    m_state.pr.predicateValue  = PredicateValue;

    static bool s_errorShown = false;

    if (pPredicate && !std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetPredication: Stub");
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GetPredication(
          ID3D11Predicate**                 ppPredicate,
          BOOL*                             pPredicateValue) {
    D3D10DeviceLock lock = LockContext();

    if (ppPredicate)
      *ppPredicate = D3D11Query::AsPredicate(m_state.pr.predicateObject.ref());

    if (pPredicateValue)
      *pPredicateValue = m_state.pr.predicateValue;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SetResourceMinLOD(
          ID3D11Resource*                   pResource,
          FLOAT                             MinLOD) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetResourceMinLOD: Not implemented");
  }
  
  
  template<typename ContextType>
  FLOAT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GetResourceMinLOD(ID3D11Resource* pResource) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::GetResourceMinLOD: Not implemented");

    return 0.0f;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopyTiles(
          ID3D11Resource*                   pTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          ID3D11Buffer*                     pBuffer,
          UINT64                            BufferStartOffsetInBytes,
          UINT                              Flags) {
    D3D10DeviceLock lock = LockContext();

    if (!pTiledResource || !pBuffer)
      return;

    auto buffer = static_cast<D3D11Buffer*>(pBuffer);

    // Get buffer slice and just forward the call
    VkDeviceSize bufferSize = pTileRegionSize->NumTiles * SparseMemoryPageSize;

    if (buffer->Desc()->ByteWidth < BufferStartOffsetInBytes + bufferSize)
      return;

    DxvkBufferSlice slice = buffer->GetBufferSlice(BufferStartOffsetInBytes, bufferSize);

    CopyTiledResourceData(pTiledResource,
      pTileRegionStartCoordinate,
      pTileRegionSize, slice, Flags);

    if (buffer->HasSequenceNumber())
      GetTypedContext()->TrackBufferSequenceNumber(buffer);
  }


  template<typename ContextType>
  HRESULT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CopyTileMappings(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestRegionCoordinate,
          ID3D11Resource*                   pSourceTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pSourceRegionCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          UINT                              Flags) {
    D3D10DeviceLock lock = LockContext();

    if (!pDestTiledResource || !pSourceTiledResource)
      return E_INVALIDARG;

    if constexpr (!IsDeferred)
      GetTypedContext()->ConsiderFlush(GpuFlushType::ImplicitWeakHint);

    DxvkSparseBindInfo bindInfo;
    bindInfo.dstResource = GetPagedResource(pDestTiledResource);
    bindInfo.srcResource = GetPagedResource(pSourceTiledResource);

    auto dstPageTable = bindInfo.dstResource->getSparsePageTable();
    auto srcPageTable = bindInfo.srcResource->getSparsePageTable();

    if (!dstPageTable || !srcPageTable)
      return E_INVALIDARG;

    if (pDestRegionCoordinate->Subresource >= dstPageTable->getSubresourceCount()
     || pSourceRegionCoordinate->Subresource >= srcPageTable->getSubresourceCount())
      return E_INVALIDARG;

    VkOffset3D dstRegionOffset = {
      int32_t(pDestRegionCoordinate->X),
      int32_t(pDestRegionCoordinate->Y),
      int32_t(pDestRegionCoordinate->Z) };

    VkOffset3D srcRegionOffset = {
      int32_t(pSourceRegionCoordinate->X),
      int32_t(pSourceRegionCoordinate->Y),
      int32_t(pSourceRegionCoordinate->Z) };

    VkExtent3D regionExtent = {
      uint32_t(pTileRegionSize->Width),
      uint32_t(pTileRegionSize->Height),
      uint32_t(pTileRegionSize->Depth) };

    for (uint32_t i = 0; i < pTileRegionSize->NumTiles; i++) {
      // We don't know the current tile mappings of either resource since
      // this may be called on a deferred context and tile mappings are
      // updated on the CS thread, so just resolve the copy in the backend
      uint32_t dstTile = dstPageTable->computePageIndex(
        pDestRegionCoordinate->Subresource, dstRegionOffset,
        regionExtent, !pTileRegionSize->bUseBox, i);

      uint32_t srcTile = srcPageTable->computePageIndex(
        pSourceRegionCoordinate->Subresource, srcRegionOffset,
        regionExtent, !pTileRegionSize->bUseBox, i);

      if (dstTile >= dstPageTable->getPageCount()
       || srcTile >= srcPageTable->getPageCount())
        return E_INVALIDARG;

      DxvkSparseBind bind;
      bind.mode = DxvkSparseBindMode::Copy;
      bind.dstPage = dstTile;
      bind.srcPage = srcTile;

      bindInfo.binds.push_back(bind);
    }

    DxvkSparseBindFlags flags = (Flags & D3D11_TILE_MAPPING_NO_OVERWRITE)
      ? DxvkSparseBindFlags(DxvkSparseBindFlag::SkipSynchronization)
      : DxvkSparseBindFlags();

    EmitCs([
      cBindInfo = std::move(bindInfo),
      cFlags    = flags
    ] (DxvkContext* ctx) {
      ctx->updatePageTable(cBindInfo, cFlags);
    });

    return S_OK;
  }


  template<typename ContextType>
  HRESULT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::ResizeTilePool(
          ID3D11Buffer*                     pTilePool,
          UINT64                            NewSizeInBytes) {
    D3D10DeviceLock lock = LockContext();

    if (NewSizeInBytes % SparseMemoryPageSize)
      return E_INVALIDARG;

    auto buffer = static_cast<D3D11Buffer*>(pTilePool);

    if (!buffer->IsTilePool())
      return E_INVALIDARG;

    // Perform the resize operation. This is somewhat trivialized
    // since all lifetime tracking is done by the backend.
    EmitCs([
      cAllocator  = buffer->GetSparseAllocator(),
      cPageCount  = NewSizeInBytes / SparseMemoryPageSize
    ] (DxvkContext* ctx) {
      cAllocator->setCapacity(cPageCount);
    });

    return S_OK;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::TiledResourceBarrier(
          ID3D11DeviceChild*                pTiledResourceOrViewAccessBeforeBarrier,
          ID3D11DeviceChild*                pTiledResourceOrViewAccessAfterBarrier) {
    D3D10DeviceLock lock = LockContext();

    DxvkGlobalPipelineBarrier srcBarrier = GetTiledResourceDependency(pTiledResourceOrViewAccessBeforeBarrier);
    DxvkGlobalPipelineBarrier dstBarrier = GetTiledResourceDependency(pTiledResourceOrViewAccessAfterBarrier);

    if (srcBarrier.stages && dstBarrier.stages) {
      EmitCs([
        cSrcBarrier = srcBarrier,
        cDstBarrier = dstBarrier
      ] (DxvkContext* ctx) {
        ctx->emitGraphicsBarrier(
          cSrcBarrier.stages, cSrcBarrier.access,
          cDstBarrier.stages, cDstBarrier.access);
      });
    }
  }


  template<typename ContextType>
  HRESULT STDMETHODCALLTYPE D3D11CommonContext<ContextType>::UpdateTileMappings(
          ID3D11Resource*                   pTiledResource,
          UINT                              NumRegions,
    const D3D11_TILED_RESOURCE_COORDINATE*  pRegionCoordinates,
    const D3D11_TILE_REGION_SIZE*           pRegionSizes,
          ID3D11Buffer*                     pTilePool,
          UINT                              NumRanges,
    const UINT*                             pRangeFlags,
    const UINT*                             pRangeTileOffsets,
    const UINT*                             pRangeTileCounts,
          UINT                              Flags) {
    D3D10DeviceLock lock = LockContext();

    if (!pTiledResource || !NumRegions || !NumRanges)
      return E_INVALIDARG;

    if constexpr (!IsDeferred)
      GetTypedContext()->ConsiderFlush(GpuFlushType::ImplicitWeakHint);

    // Find sparse allocator if the tile pool is defined
    DxvkSparseBindInfo bindInfo;

    if (pTilePool) {
      auto tilePool = static_cast<D3D11Buffer*>(pTilePool);
      bindInfo.srcAllocator = tilePool->GetSparseAllocator();

      if (bindInfo.srcAllocator == nullptr)
        return E_INVALIDARG;
    }

    // Find resource and sparse page table for the given resource
    bindInfo.dstResource = GetPagedResource(pTiledResource);
    auto pageTable = bindInfo.dstResource->getSparsePageTable();

    if (!pageTable)
      return E_INVALIDARG;

    // Lookup table in case the app tries to bind the same
    // page multiple times. We should resolve that here and
    // only consider the last bind to any given page.
    std::vector<uint32_t> bindIndices(pageTable->getPageCount(), ~0u);

    // This function allows pretty much every parameter to be nullptr
    // in some way, so initialize some defaults as necessary
    D3D11_TILED_RESOURCE_COORDINATE regionCoord = { };
    D3D11_TILE_REGION_SIZE regionSize = { };

    if (!pRegionSizes) {
      regionSize.NumTiles = pRegionCoordinates
        ? 1 : pageTable->getPageCount();
    }

    uint32_t rangeFlag = 0u;
    uint32_t rangeTileOffset = 0u;
    uint32_t rangeTileCount = ~0u;

    // For now, just generate a simple list of tile index to
    // page index mappings, and let the backend optimize later
    uint32_t regionIdx = 0u;
    uint32_t regionTile = 0u;
    uint32_t rangeIdx = 0u;
    uint32_t rangeTile = 0u;

    while (regionIdx < NumRegions && rangeIdx < NumRanges) {
      if (!regionTile) {
        if (pRegionCoordinates)
          regionCoord = pRegionCoordinates[regionIdx];

        if (pRegionSizes)
          regionSize = pRegionSizes[regionIdx];
      }

      if (!rangeTile) {
        if (pRangeFlags)
          rangeFlag = pRangeFlags[rangeIdx];

        if (pRangeTileOffsets)
          rangeTileOffset = pRangeTileOffsets[rangeIdx];

        if (pRangeTileCounts)
          rangeTileCount = pRangeTileCounts[rangeIdx];
      }

      if (!(rangeFlag & D3D11_TILE_RANGE_SKIP)) {
        if (regionCoord.Subresource >= pageTable->getSubresourceCount())
          return E_INVALIDARG;

        if (regionSize.bUseBox && regionSize.NumTiles !=
            regionSize.Width * regionSize.Height * regionSize.Depth)
          return E_INVALIDARG;

        VkOffset3D regionOffset = {
          int32_t(regionCoord.X),
          int32_t(regionCoord.Y),
          int32_t(regionCoord.Z) };

        VkExtent3D regionExtent = {
          uint32_t(regionSize.Width),
          uint32_t(regionSize.Height),
          uint32_t(regionSize.Depth) };

        uint32_t resourceTile = pageTable->computePageIndex(regionCoord.Subresource,
          regionOffset, regionExtent, !regionSize.bUseBox, regionTile);

        // Fill in bind info for the current tile
        DxvkSparseBind bind = { };
        bind.dstPage = resourceTile;

        if (rangeFlag & D3D11_TILE_RANGE_NULL) {
          bind.mode = DxvkSparseBindMode::Null;
        } else if (pTilePool) {
          bind.mode = DxvkSparseBindMode::Bind;
          bind.srcPage = rangeFlag & D3D11_TILE_RANGE_REUSE_SINGLE_TILE
            ? rangeTileOffset
            : rangeTileOffset + rangeTile;
        } else {
          return E_INVALIDARG;
        }

        // Add bind info to the bind list, overriding
        // any existing bind for the same resource page
        if (resourceTile < pageTable->getPageCount()) {
          if (bindIndices[resourceTile] < bindInfo.binds.size())
            bindInfo.binds[bindIndices[resourceTile]] = bind;
          else
            bindInfo.binds.push_back(bind);
        }
      }

      if (++regionTile == regionSize.NumTiles) {
        regionTile = 0;
        regionIdx += 1;
      }

      if (++rangeTile == rangeTileCount) {
        rangeTile = 0;
        rangeIdx += 1;
      }
    }

    // Translate flags. The backend benefits from NO_OVERWRITE since
    // otherwise we have to serialize execution of the current command
    // buffer, the sparse binding operation, and subsequent commands.
    // With NO_OVERWRITE, we can execute it more or less asynchronously.
    DxvkSparseBindFlags flags = (Flags & D3D11_TILE_MAPPING_NO_OVERWRITE)
      ? DxvkSparseBindFlags(DxvkSparseBindFlag::SkipSynchronization)
      : DxvkSparseBindFlags();

    EmitCs([
      cBindInfo = std::move(bindInfo),
      cFlags    = flags
    ] (DxvkContext* ctx) {
      ctx->updatePageTable(cBindInfo, cFlags);
    });

    return S_OK;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::UpdateTiles(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pDestTileRegionSize,
    const void*                             pSourceTileData,
          UINT                              Flags) {
    D3D10DeviceLock lock = LockContext();

    if (!pDestTiledResource || !pSourceTileData)
      return;

    // Allocate staging memory and copy source data into it, at a
    // 64k page granularity. It is not clear whether this behaviour
    // is correct in case we're writing to incmplete pages.
    VkDeviceSize bufferSize = pDestTileRegionSize->NumTiles * SparseMemoryPageSize;

    DxvkBufferSlice slice = AllocStagingBuffer(bufferSize);
    std::memcpy(slice.mapPtr(0), pSourceTileData, bufferSize);

    // Fix up flags. The runtime probably validates this in some
    // way but our internal function relies on correct flags anyway.
    Flags &= D3D11_TILE_MAPPING_NO_OVERWRITE;
    Flags |= D3D11_TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE;

    CopyTiledResourceData(pDestTiledResource,
      pDestTileRegionStartCoordinate,
      pDestTileRegionSize, slice, Flags);
  }


  template<typename ContextType>
  BOOL STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IsAnnotationEnabled() {
    return m_annotation.GetStatus();
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SetMarkerInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::BeginEventInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::EndEvent() {
    // Not implemented in the backend, ignore
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GetHardwareProtectionState(
          BOOL*                             pHwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::GetHardwareProtectionState: Not implemented");

    if (pHwProtectionEnable)
      *pHwProtectionEnable = FALSE;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::SetHardwareProtectionState(
          BOOL                              HwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetHardwareProtectionState: Not implemented");
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::TransitionSurfaceLayout(
          IDXGIVkInteropSurface*    pSurface,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) {
    D3D10DeviceLock lock = LockContext();

    // Get the underlying D3D11 resource
    Com<ID3D11Resource> resource;

    pSurface->QueryInterface(__uuidof(ID3D11Resource),
      reinterpret_cast<void**>(&resource));

    // Get the texture from that resource
    D3D11CommonTexture* texture = GetCommonTexture(resource.ptr());

    EmitCs([
      cImage        = texture->GetImage(),
      cSubresources = *pSubresources,
      cOldLayout    = OldLayout,
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->transformImage(
        cImage, cSubresources,
        cOldLayout, cNewLayout);
    });
  }


  template<typename ContextType>
  DxvkCsChunkRef D3D11CommonContext<ContextType>::AllocCsChunk() {
    return m_parent->AllocCsChunk(m_csFlags);
  }


  template<typename ContextType>
  DxvkDataSlice D3D11CommonContext<ContextType>::AllocUpdateBufferSlice(size_t Size) {
    constexpr size_t UpdateBufferSize = 1 * 1024 * 1024;
    
    if (Size >= UpdateBufferSize) {
      Rc<DxvkDataBuffer> buffer = new DxvkDataBuffer(Size);
      return buffer->alloc(Size);
    } else {
      if (m_updateBuffer == nullptr)
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
      
      DxvkDataSlice slice = m_updateBuffer->alloc(Size);
      
      if (slice.ptr() == nullptr) {
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
        slice = m_updateBuffer->alloc(Size);
      }
      
      return slice;
    }
  }


  template<typename ContextType>
  DxvkBufferSlice D3D11CommonContext<ContextType>::AllocStagingBuffer(
          VkDeviceSize                      Size) {
    return m_staging.alloc(256, Size);
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyInputLayout() {
    auto inputLayout = m_state.ia.inputLayout.prvRef();

    if (likely(inputLayout != nullptr)) {
      EmitCs([
        cInputLayout = std::move(inputLayout)
      ] (DxvkContext* ctx) {
        cInputLayout->BindToContext(ctx);
      });
    } else {
      EmitCs([] (DxvkContext* ctx) {
        ctx->setInputLayout(0, nullptr, 0, nullptr);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyPrimitiveTopology() {
    D3D11_PRIMITIVE_TOPOLOGY topology = m_state.ia.primitiveTopology;
    DxvkInputAssemblyState iaState = { };

    if (topology <= D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ) {
      static const std::array<DxvkInputAssemblyState, 14> s_iaStates = {{
        { VK_PRIMITIVE_TOPOLOGY_MAX_ENUM,       VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     VK_TRUE,  0 },
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE,  0 },
        { }, { }, { }, { }, // Random gap that exists for no reason
        { VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,       VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,      VK_TRUE,  0 },
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,   VK_FALSE, 0 },
        { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,  VK_TRUE,  0 },
      }};

      iaState = s_iaStates[uint32_t(topology)];
    } else if (topology >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
            && topology <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST) {
      // The number of control points per patch can be inferred from the enum value in D3D11
      uint32_t vertexCount = uint32_t(topology - D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1);
      iaState = { VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE, vertexCount };
    }
    
    EmitCs([iaState] (DxvkContext* ctx) {
      ctx->setInputAssemblyState(iaState);
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyBlendState() {
    if (m_state.om.cbState != nullptr) {
      EmitCs([
        cBlendState = m_state.om.cbState,
        cSampleMask = m_state.om.sampleMask
      ] (DxvkContext* ctx) {
        cBlendState->BindToContext(ctx, cSampleMask);
      });
    } else {
      EmitCs([
        cSampleMask = m_state.om.sampleMask
      ] (DxvkContext* ctx) {
        DxvkBlendMode cbState;
        DxvkLogicOpState loState;
        DxvkMultisampleState msState;
        InitDefaultBlendState(&cbState, &loState, &msState, cSampleMask);

        for (uint32_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
          ctx->setBlendMode(i, cbState);

        ctx->setLogicOpState(loState);
        ctx->setMultisampleState(msState);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyBlendFactor() {
    EmitCs([
      cBlendConstants = DxvkBlendConstants {
        m_state.om.blendFactor[0], m_state.om.blendFactor[1],
        m_state.om.blendFactor[2], m_state.om.blendFactor[3] }
    ] (DxvkContext* ctx) {
      ctx->setBlendConstants(cBlendConstants);
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyDepthStencilState() {
    if (m_state.om.dsState != nullptr) {
      EmitCs([
        cDepthStencilState = m_state.om.dsState
      ] (DxvkContext* ctx) {
        cDepthStencilState->BindToContext(ctx);
      });
    } else {
      EmitCs([] (DxvkContext* ctx) {
        DxvkDepthStencilState dsState;
        InitDefaultDepthStencilState(&dsState);

        ctx->setDepthStencilState(dsState);
      });
    }
  }
  
  
  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyStencilRef() {
    EmitCs([
      cStencilRef = m_state.om.stencilRef
    ] (DxvkContext* ctx) {
      ctx->setStencilReference(cStencilRef);
    });
  }
  
  
  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyRasterizerState() {
    if (m_state.rs.state != nullptr) {
      EmitCs([
        cRasterizerState = m_state.rs.state
      ] (DxvkContext* ctx) {
        cRasterizerState->BindToContext(ctx);
      });
    } else {
      EmitCs([] (DxvkContext* ctx) {
        DxvkRasterizerState rsState;
        InitDefaultRasterizerState(&rsState);

        ctx->setRasterizerState(rsState);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyRasterizerSampleCount() {
    DxbcPushConstants pc;
    pc.rasterizerSampleCount = m_state.om.sampleCount;

    if (unlikely(!m_state.om.sampleCount)) {
      pc.rasterizerSampleCount = m_state.rs.state
        ? m_state.rs.state->Desc()->ForcedSampleCount
        : 0;

      if (!pc.rasterizerSampleCount)
        pc.rasterizerSampleCount = 1;
    }

    EmitCs([
      cPushConstants = pc
    ] (DxvkContext* ctx) {
      ctx->pushConstants(0, sizeof(cPushConstants), &cPushConstants);
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ApplyViewportState() {
    std::array<VkViewport, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
    std::array<VkRect2D,   D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;

    // The backend can't handle a viewport count of zero,
    // so we should at least specify one empty viewport
    uint32_t viewportCount = m_state.rs.numViewports;

    if (unlikely(!viewportCount)) {
      viewportCount = 1;
      viewports[0] = VkViewport();
      scissors [0] = VkRect2D();
    }

    // D3D11's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    for (uint32_t i = 0; i < m_state.rs.numViewports; i++) {
      const D3D11_VIEWPORT& vp = m_state.rs.viewports[i];

      viewports[i] = VkViewport {
        vp.TopLeftX, vp.Height + vp.TopLeftY,
        vp.Width,   -vp.Height,
        vp.MinDepth, vp.MaxDepth,
      };
    }

    // Scissor rectangles. Vulkan does not provide an easy way
    // to disable the scissor test, so we'll have to set scissor
    // rects that are at least as large as the framebuffer.
    bool enableScissorTest = false;
  
    if (m_state.rs.state != nullptr) {
      D3D11_RASTERIZER_DESC rsDesc;
      m_state.rs.state->GetDesc(&rsDesc);
      enableScissorTest = rsDesc.ScissorEnable;
    }

    for (uint32_t i = 0; i < m_state.rs.numViewports; i++) {
      if (!enableScissorTest) {
        scissors[i] = VkRect2D {
          VkOffset2D { 0, 0 },
          VkExtent2D {
            D3D11_VIEWPORT_BOUNDS_MAX,
            D3D11_VIEWPORT_BOUNDS_MAX } };
      } else if (i >= m_state.rs.numScissors) {
        scissors[i] = VkRect2D {
          VkOffset2D { 0, 0 },
          VkExtent2D { 0, 0 } };
      } else {
        D3D11_RECT sr = m_state.rs.scissors[i];

        VkOffset2D srPosA;
        srPosA.x = std::max<int32_t>(0, sr.left);
        srPosA.y = std::max<int32_t>(0, sr.top);

        VkOffset2D srPosB;
        srPosB.x = std::max<int32_t>(srPosA.x, sr.right);
        srPosB.y = std::max<int32_t>(srPosA.y, sr.bottom);

        VkExtent2D srSize;
        srSize.width  = uint32_t(srPosB.x - srPosA.x);
        srSize.height = uint32_t(srPosB.y - srPosA.y);

        scissors[i] = VkRect2D { srPosA, srSize };
      }
    }

    if (likely(viewportCount == 1)) {
      EmitCs([
        cViewport = viewports[0],
        cScissor  = scissors[0]
      ] (DxvkContext* ctx) {
        ctx->setViewports(1,
          &cViewport,
          &cScissor);
      });
    } else {
      EmitCs([
        cViewportCount = viewportCount,
        cViewports     = viewports,
        cScissors      = scissors
      ] (DxvkContext* ctx) {
        ctx->setViewports(
          cViewportCount,
          cViewports.data(),
          cScissors.data());
      });
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindShader(
    const D3D11CommonShader*    pShaderModule) {
    if (pShaderModule) {
      auto buffer = pShaderModule->GetIcb();
      auto shader = pShaderModule->GetShader();

      if (unlikely(shader->needsLibraryCompile()))
        m_device->requestCompileShader(shader);

      EmitCs([
        cBuffer = std::move(buffer),
        cShader = std::move(shader)
      ] (DxvkContext* ctx) mutable {
        constexpr VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);

        uint32_t slotId = computeConstantBufferBinding(ShaderStage,
          D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

        ctx->bindShader<stage>(
          Forwarder::move(cShader));
        ctx->bindUniformBuffer(stage, slotId,
          Forwarder::move(cBuffer));
      });
    } else {
      EmitCs([] (DxvkContext* ctx) {
        constexpr VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);

        uint32_t slotId = computeConstantBufferBinding(ShaderStage,
          D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

        ctx->bindShader<stage>(nullptr);
        ctx->bindUniformBuffer(stage, slotId, DxvkBufferSlice());
      });
    }
  }


  static VkDepthBiasRepresentationEXT FormatToDepthBiasRepresentation(DXGI_FORMAT format) {
    switch (format) {
      default:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_D32_FLOAT:
        return VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_D16_UNORM:
        return VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT;
    }
  }

  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindFramebuffer() {
    DxvkDepthBiasRepresentation depthBiasRepresentation =
      { VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT,
        m_device->features().extDepthBiasControl.depthBiasExact };
    DxvkRenderTargets attachments;
    uint32_t sampleCount = 0;

    // D3D11 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.om.rtvs.size(); i++) {
      if (m_state.om.rtvs[i] != nullptr) {
        attachments.color[i] = {
          m_state.om.rtvs[i]->GetImageView(),
          m_state.om.rtvs[i]->GetRenderLayout() };
        sampleCount = m_state.om.rtvs[i]->GetSampleCount();
      }
    }

    if (m_state.om.dsv != nullptr) {
      attachments.depth = {
        m_state.om.dsv->GetImageView(),
        m_state.om.dsv->GetRenderLayout() };
      sampleCount = m_state.om.dsv->GetSampleCount();

      if (m_device->features().extDepthBiasControl.leastRepresentableValueForceUnormRepresentation)
        depthBiasRepresentation.depthBiasRepresentation = FormatToDepthBiasRepresentation(m_state.om.dsv->GetViewFormat());
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments    = std::move(attachments),
      cRepresentation = depthBiasRepresentation
    ] (DxvkContext* ctx) mutable {
      ctx->setDepthBiasRepresentation(cRepresentation);
      ctx->bindRenderTargets(Forwarder::move(cAttachments), 0u);
    });

    // If necessary, update push constant for the sample count
    if (m_state.om.sampleCount != sampleCount) {
      m_state.om.sampleCount = sampleCount;
      ApplyRasterizerSampleCount();
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindDrawBuffers(
          D3D11Buffer*                     pBufferForArgs,
          D3D11Buffer*                     pBufferForCount) {
    EmitCs([
      cArgBuffer = pBufferForArgs  ? pBufferForArgs->GetBufferSlice()  : DxvkBufferSlice(),
      cCntBuffer = pBufferForCount ? pBufferForCount->GetBufferSlice() : DxvkBufferSlice()
    ] (DxvkContext* ctx) mutable {
      ctx->bindDrawBuffers(
        Forwarder::move(cArgBuffer),
        Forwarder::move(cCntBuffer));
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindVertexBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Stride) {
    if (pBuffer) {
      EmitCs([
        cSlotId       = Slot,
        cBufferSlice  = pBuffer->GetBufferSlice(Offset),
        cStride       = Stride
      ] (DxvkContext* ctx) mutable {
        ctx->bindVertexBuffer(cSlotId,
          Forwarder::move(cBufferSlice),
          cStride);
      });
    } else {
      EmitCs([
        cSlotId       = Slot
      ] (DxvkContext* ctx) {
        ctx->bindVertexBuffer(cSlotId, DxvkBufferSlice(), 0);
      });
    }
  }
  
  
  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindVertexBufferRange(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Stride) {
    if (pBuffer) {
      VkDeviceSize offset = Offset;
      VkDeviceSize length = pBuffer->GetRemainingSize(Offset);

      EmitCs([
        cSlotId       = Slot,
        cBufferOffset = offset,
        cBufferLength = length,
        cStride       = Stride
      ] (DxvkContext* ctx) mutable {
        ctx->bindVertexBufferRange(cSlotId,
          cBufferOffset, cBufferLength, cStride);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindIndexBuffer(
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          DXGI_FORMAT                       Format) {
    VkIndexType indexType = Format == DXGI_FORMAT_R16_UINT
      ? VK_INDEX_TYPE_UINT16
      : VK_INDEX_TYPE_UINT32;

    if (pBuffer) {
      EmitCs([
        cBufferSlice  = pBuffer->GetBufferSlice(Offset),
        cIndexType    = indexType
      ] (DxvkContext* ctx) mutable {
        ctx->bindIndexBuffer(
          Forwarder::move(cBufferSlice),
          cIndexType);
      });
    } else {
      EmitCs([
        cIndexType    = indexType
      ] (DxvkContext* ctx) {
        ctx->bindIndexBuffer(DxvkBufferSlice(), cIndexType);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindIndexBufferRange(
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          DXGI_FORMAT                       Format) {
    if (pBuffer) {
      VkIndexType indexType = Format == DXGI_FORMAT_R16_UINT
        ? VK_INDEX_TYPE_UINT16
        : VK_INDEX_TYPE_UINT32;

      VkDeviceSize offset = Offset;
      VkDeviceSize length = pBuffer->GetRemainingSize(Offset);

      EmitCs([
        cBufferOffset = offset,
        cBufferLength = length,
        cIndexType    = indexType
      ] (DxvkContext* ctx) mutable {
        ctx->bindIndexBufferRange(
          cBufferOffset, cBufferLength,
          cIndexType);
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindXfbBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset) {
    if (pBuffer) {
      EmitCs([
        cSlotId       = Slot,
        cOffset       = Offset,
        cBufferSlice  = pBuffer->GetBufferSlice(),
        cCounterSlice = pBuffer->GetSOCounter()
      ] (DxvkContext* ctx) mutable {
        if (cCounterSlice.defined() && cOffset != ~0u) {
          ctx->updateBuffer(
            cCounterSlice.buffer(),
            cCounterSlice.offset(),
            sizeof(cOffset),
            &cOffset);
        }

        ctx->bindXfbBuffer(cSlotId,
          Forwarder::move(cBufferSlice),
          Forwarder::move(cCounterSlice));
      });
    } else {
      EmitCs([
        cSlotId       = Slot
      ] (DxvkContext* ctx) {
        ctx->bindXfbBuffer(cSlotId,
          DxvkBufferSlice(),
          DxvkBufferSlice());
      });
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindConstantBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Length) {
    if (pBuffer) {
      EmitCs([
        cSlotId      = Slot,
        cBufferSlice = pBuffer->GetBufferSlice(16 * Offset, 16 * Length)
      ] (DxvkContext* ctx) mutable {
        VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
        ctx->bindUniformBuffer(stage, cSlotId,
          Forwarder::move(cBufferSlice));
      });
    } else {
      EmitCs([
        cSlotId      = Slot
      ] (DxvkContext* ctx) {
        VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
        ctx->bindUniformBuffer(stage, cSlotId, DxvkBufferSlice());
      });
    }
  }
  
  
  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindConstantBufferRange(
          UINT                              Slot,
          UINT                              Offset,
          UINT                              Length) {
    EmitCs([
      cSlotId       = Slot,
      cOffset       = 16 * Offset,
      cLength       = 16 * Length
    ] (DxvkContext* ctx) {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
      ctx->bindUniformBufferRange(stage, cSlotId, cOffset, cLength);
    });
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindSampler(
          UINT                              Slot,
          D3D11SamplerState*                pSampler) {
    if (pSampler) {
      EmitCs([
        cSlotId   = Slot,
        cSampler  = pSampler->GetDXVKSampler()
      ] (DxvkContext* ctx) mutable {
        VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
        ctx->bindResourceSampler(stage, cSlotId,
          Forwarder::move(cSampler));
      });
    } else {
      EmitCs([
        cSlotId   = Slot
      ] (DxvkContext* ctx) {
        VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
        ctx->bindResourceSampler(stage, cSlotId, nullptr);
      });
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindShaderResource(
          UINT                              Slot,
          D3D11ShaderResourceView*          pResource) {
    if (pResource) {
      if (pResource->GetViewInfo().Dimension != D3D11_RESOURCE_DIMENSION_BUFFER) {
        EmitCs([
          cSlotId = Slot,
          cView   = pResource->GetImageView()
        ] (DxvkContext* ctx) mutable {
          VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
          ctx->bindResourceImageView(stage, cSlotId,
            Forwarder::move(cView));
        });
      } else {
        EmitCs([
          cSlotId = Slot,
          cView   = pResource->GetBufferView()
        ] (DxvkContext* ctx) mutable {
          VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
          ctx->bindResourceBufferView(stage, cSlotId,
            Forwarder::move(cView));
        });
      }
    } else {
      EmitCs([
        cSlotId = Slot
      ] (DxvkContext* ctx) {
        VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
        ctx->bindResourceImageView(stage, cSlotId, nullptr);
      });
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindUnorderedAccessView(
          UINT                              UavSlot,
          D3D11UnorderedAccessView*         pUav,
          UINT                              CtrSlot,
          UINT                              Counter) {
    if (pUav) {
      if (pUav->GetViewInfo().Dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
        EmitCs([
          cUavSlotId    = UavSlot,
          cCtrSlotId    = CtrSlot,
          cBufferView   = pUav->GetBufferView(),
          cCounterView  = pUav->GetCounterView(),
          cCounterValue = Counter
        ] (DxvkContext* ctx) mutable {
          VkShaderStageFlags stages = ShaderStage == DxbcProgramType::ComputeShader
            ? VK_SHADER_STAGE_COMPUTE_BIT
            : VK_SHADER_STAGE_ALL_GRAPHICS;

          if (cCounterView != nullptr && cCounterValue != ~0u) {
            auto counterSlice = cCounterView->slice();

            ctx->updateBuffer(
              counterSlice.buffer(),
              counterSlice.offset(),
              sizeof(uint32_t),
              &cCounterValue);
          }

          ctx->bindResourceBufferView(stages, cUavSlotId,
            Forwarder::move(cBufferView));
          ctx->bindResourceBufferView(stages, cCtrSlotId,
            Forwarder::move(cCounterView));
        });
      } else {
        EmitCs([
          cUavSlotId    = UavSlot,
          cCtrSlotId    = CtrSlot,
          cImageView    = pUav->GetImageView()
        ] (DxvkContext* ctx) mutable {
          VkShaderStageFlags stages = ShaderStage == DxbcProgramType::ComputeShader
            ? VK_SHADER_STAGE_COMPUTE_BIT
            : VK_SHADER_STAGE_ALL_GRAPHICS;

          ctx->bindResourceImageView(stages, cUavSlotId,
            Forwarder::move(cImageView));
          ctx->bindResourceBufferView(stages, cCtrSlotId, nullptr);
        });
      }
    } else {
      EmitCs([
        cUavSlotId    = UavSlot,
        cCtrSlotId    = CtrSlot
      ] (DxvkContext* ctx) {
        VkShaderStageFlags stages = ShaderStage == DxbcProgramType::ComputeShader
          ? VK_SHADER_STAGE_COMPUTE_BIT
          : VK_SHADER_STAGE_ALL_GRAPHICS;

        ctx->bindResourceImageView(stages, cUavSlotId, nullptr);
        ctx->bindResourceBufferView(stages, cCtrSlotId, nullptr);
      });
    }
  }


  template<typename ContextType>
  VkClearValue D3D11CommonContext<ContextType>::ConvertColorValue(
    const FLOAT                             Color[4],
    const DxvkFormatInfo*                   pFormatInfo) {
    VkClearValue result;

    if (pFormatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      for (uint32_t i = 0; i < 4; i++) {
        if (pFormatInfo->flags.test(DxvkFormatFlag::SampledUInt))
          result.color.uint32[i] = uint32_t(std::max(0.0f, Color[i]));
        else if (pFormatInfo->flags.test(DxvkFormatFlag::SampledSInt))
          result.color.int32[i] = int32_t(Color[i]);
        else
          result.color.float32[i] = Color[i];
      }
    } else {
      result.depthStencil.depth = Color[0];
      result.depthStencil.stencil = 0;
    }

    return result;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::CopyBuffer(
          D3D11Buffer*                      pDstBuffer,
          VkDeviceSize                      DstOffset,
          D3D11Buffer*                      pSrcBuffer,
          VkDeviceSize                      SrcOffset,
          VkDeviceSize                      ByteCount) {
    // Clamp copy region to prevent out-of-bounds access
    VkDeviceSize dstLength = pDstBuffer->Desc()->ByteWidth;
    VkDeviceSize srcLength = pSrcBuffer->Desc()->ByteWidth;

    if (SrcOffset >= srcLength || DstOffset >= dstLength || !ByteCount)
      return;

    ByteCount = std::min(dstLength - DstOffset, ByteCount);
    ByteCount = std::min(srcLength - SrcOffset, ByteCount);

    EmitCs([
      cDstBuffer = pDstBuffer->GetBufferSlice(DstOffset, ByteCount),
      cSrcBuffer = pSrcBuffer->GetBufferSlice(SrcOffset, ByteCount)
    ] (DxvkContext* ctx) {
      if (cDstBuffer.buffer() != cSrcBuffer.buffer()) {
        ctx->copyBuffer(
          cDstBuffer.buffer(),
          cDstBuffer.offset(),
          cSrcBuffer.buffer(),
          cSrcBuffer.offset(),
          cSrcBuffer.length());
      } else {
        ctx->copyBufferRegion(
          cDstBuffer.buffer(),
          cDstBuffer.offset(),
          cSrcBuffer.offset(),
          cSrcBuffer.length());
      }
    });

    if (pDstBuffer->HasSequenceNumber())
      GetTypedContext()->TrackBufferSequenceNumber(pDstBuffer);
    if (pSrcBuffer->HasSequenceNumber())
      GetTypedContext()->TrackBufferSequenceNumber(pSrcBuffer);
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::CopyImage(
          D3D11CommonTexture*               pDstTexture,
    const VkImageSubresourceLayers*         pDstLayers,
          VkOffset3D                        DstOffset,
          D3D11CommonTexture*               pSrcTexture,
    const VkImageSubresourceLayers*         pSrcLayers,
          VkOffset3D                        SrcOffset,
          VkExtent3D                        SrcExtent) {
    // Image formats must be size-compatible
    auto dstFormatInfo = lookupFormatInfo(pDstTexture->GetPackedFormat());
    auto srcFormatInfo = lookupFormatInfo(pSrcTexture->GetPackedFormat());

    if (dstFormatInfo->elementSize != srcFormatInfo->elementSize)
      return;

    // Sample counts must match
    if (pDstTexture->Desc()->SampleDesc.Count != pSrcTexture->Desc()->SampleDesc.Count)
      return;

    // Obviously, the copy region must not be empty
    VkExtent3D dstMipExtent = pDstTexture->MipLevelExtent(pDstLayers->mipLevel);
    VkExtent3D srcMipExtent = pSrcTexture->MipLevelExtent(pSrcLayers->mipLevel);

    if (uint32_t(DstOffset.x) >= dstMipExtent.width
     || uint32_t(DstOffset.y) >= dstMipExtent.height
     || uint32_t(DstOffset.z) >= dstMipExtent.depth)
      return;

    if (uint32_t(SrcOffset.x) >= srcMipExtent.width
     || uint32_t(SrcOffset.y) >= srcMipExtent.height
     || uint32_t(SrcOffset.z) >= srcMipExtent.depth)
      return;

    // Don't perform the copy if the offsets aren't block-aligned
    if (!util::isBlockAligned(SrcOffset, srcFormatInfo->blockSize)
     || !util::isBlockAligned(DstOffset, dstFormatInfo->blockSize))
      return;

    // Clamp the image region in order to avoid out-of-bounds access
    VkExtent3D blockCount    = util::computeBlockCount(SrcExtent, srcFormatInfo->blockSize);
    VkExtent3D dstBlockCount = util::computeMaxBlockCount(DstOffset, dstMipExtent, dstFormatInfo->blockSize);
    VkExtent3D srcBlockCount = util::computeMaxBlockCount(SrcOffset, srcMipExtent, srcFormatInfo->blockSize);

    blockCount = util::minExtent3D(blockCount, dstBlockCount);
    blockCount = util::minExtent3D(blockCount, srcBlockCount);

    SrcExtent = util::computeBlockExtent(blockCount, srcFormatInfo->blockSize);
    SrcExtent = util::snapExtent3D(SrcOffset, SrcExtent, srcMipExtent);

    if (!SrcExtent.width || !SrcExtent.height || !SrcExtent.depth)
      return;

    // While copying between 2D and 3D images is allowed in CopySubresourceRegion,
    // copying more than one slice at a time is not suppoted. Layer counts are 1.
    if ((pDstTexture->GetVkImageType() == VK_IMAGE_TYPE_3D)
     != (pSrcTexture->GetVkImageType() == VK_IMAGE_TYPE_3D))
      SrcExtent.depth = 1;

    // Certain types of copies require us to pass the destination extent to
    // the backend. This may be different when copying between compressed
    // and uncompressed image formats.
    VkExtent3D dstExtent = util::computeBlockExtent(blockCount, dstFormatInfo->blockSize);
    dstExtent = util::snapExtent3D(DstOffset, dstExtent, dstMipExtent);

    // It is possible for any of the given images to be a staging image with
    // no actual image, so we need to account for all possibilities here.
    bool dstIsImage = pDstTexture->GetMapMode() != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING;
    bool srcIsImage = pSrcTexture->GetMapMode() != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING;

    if (dstIsImage && srcIsImage) {
      EmitCs([
        cDstImage  = pDstTexture->GetImage(),
        cSrcImage  = pSrcTexture->GetImage(),
        cDstLayers = *pDstLayers,
        cSrcLayers = *pSrcLayers,
        cDstOffset = DstOffset,
        cSrcOffset = SrcOffset,
        cExtent    = SrcExtent
      ] (DxvkContext* ctx) {
        // CopyResource can only copy between different images, and
        // CopySubresourceRegion can only copy data from one single
        // subresource at a time, so this check is safe.
        if (cDstImage != cSrcImage || cDstLayers != cSrcLayers) {
          ctx->copyImage(
            cDstImage, cDstLayers, cDstOffset,
            cSrcImage, cSrcLayers, cSrcOffset,
            cExtent);
        } else {
          ctx->copyImageRegion(
            cDstImage, cDstLayers, cDstOffset,
            cSrcOffset, cExtent);
        }
      });
    } else {
      // Since each subresource uses a dedicated buffer, we are going
      // to need one call per subresource for staging resource copies
      for (uint32_t i = 0; i < pDstLayers->layerCount; i++) {
        uint32_t dstSubresource = D3D11CalcSubresource(pDstLayers->mipLevel, pDstLayers->baseArrayLayer + i, pDstTexture->Desc()->MipLevels);
        uint32_t srcSubresource = D3D11CalcSubresource(pSrcLayers->mipLevel, pSrcLayers->baseArrayLayer + i, pSrcTexture->Desc()->MipLevels);

        // For multi-plane image data stored in a buffer, the backend
        // assumes that the second plane immediately follows the first
        // plane in memory, which is only true if we copy the full image.
        uint32_t planeCount = 1;

        if (dstFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
          bool needsSeparateCopies = !dstIsImage && !srcIsImage;

          if (!dstIsImage)
            needsSeparateCopies |= pDstTexture->MipLevelExtent(pDstLayers->mipLevel) != SrcExtent;
          if (!srcIsImage)
            needsSeparateCopies |= pSrcTexture->MipLevelExtent(pSrcLayers->mipLevel) != SrcExtent;

          if (needsSeparateCopies)
            planeCount = vk::getPlaneCount(srcFormatInfo->aspectMask);
        }

        for (uint32_t j = 0; j < planeCount; j++) {
          VkImageAspectFlags dstAspectMask = dstFormatInfo->aspectMask;
          VkImageAspectFlags srcAspectMask = srcFormatInfo->aspectMask;

          if (planeCount > 1) {
            dstAspectMask = vk::getPlaneAspect(j);
            srcAspectMask = dstAspectMask;
          }

          if (dstIsImage) {
            VkImageSubresourceLayers dstLayer = { dstAspectMask,
              pDstLayers->mipLevel, pDstLayers->baseArrayLayer + i, 1 };

            EmitCs([
              cDstImage   = pDstTexture->GetImage(),
              cDstLayers  = dstLayer,
              cDstOffset  = DstOffset,
              cDstExtent  = dstExtent,
              cSrcBuffer  = pSrcTexture->GetMappedBuffer(srcSubresource),
              cSrcLayout  = pSrcTexture->GetSubresourceLayout(srcAspectMask, srcSubresource),
              cSrcOffset  = pSrcTexture->ComputeMappedOffset(srcSubresource, j, SrcOffset),
              cSrcCoord   = SrcOffset,
              cSrcExtent  = srcMipExtent,
              cSrcFormat  = pSrcTexture->GetPackedFormat()
            ] (DxvkContext* ctx) {
              if (cDstLayers.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
                ctx->copyBufferToImage(cDstImage, cDstLayers, cDstOffset, cDstExtent,
                  cSrcBuffer, cSrcOffset, cSrcLayout.RowPitch, cSrcLayout.DepthPitch);
              } else {
                ctx->copyPackedBufferToDepthStencilImage(cDstImage, cDstLayers,
                  VkOffset2D { cDstOffset.x,     cDstOffset.y      },
                  VkExtent2D { cDstExtent.width, cDstExtent.height },
                  cSrcBuffer, cSrcLayout.Offset,
                  VkOffset2D { cSrcCoord.x,      cSrcCoord.y       },
                  VkExtent2D { cSrcExtent.width, cSrcExtent.height },
                  cSrcFormat);
              }
            });
          } else if (srcIsImage) {
            VkImageSubresourceLayers srcLayer = { srcAspectMask,
              pSrcLayers->mipLevel, pSrcLayers->baseArrayLayer + i, 1 };

            EmitCs([
              cSrcImage   = pSrcTexture->GetImage(),
              cSrcLayers  = srcLayer,
              cSrcOffset  = SrcOffset,
              cSrcExtent  = SrcExtent,
              cDstBuffer  = pDstTexture->GetMappedBuffer(dstSubresource),
              cDstLayout  = pDstTexture->GetSubresourceLayout(dstAspectMask, dstSubresource),
              cDstOffset  = pDstTexture->ComputeMappedOffset(dstSubresource, j, DstOffset),
              cDstCoord   = DstOffset,
              cDstExtent  = dstMipExtent,
              cDstFormat  = pDstTexture->GetPackedFormat()
            ] (DxvkContext* ctx) {
              if (cSrcLayers.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
                ctx->copyImageToBuffer(cDstBuffer, cDstOffset, cDstLayout.RowPitch,
                  cDstLayout.DepthPitch, cSrcImage, cSrcLayers, cSrcOffset, cSrcExtent);
              } else {
                ctx->copyDepthStencilImageToPackedBuffer(cDstBuffer, cDstLayout.Offset,
                  VkOffset2D { cDstCoord.x,      cDstCoord.y       },
                  VkExtent2D { cDstExtent.width, cDstExtent.height },
                  cSrcImage, cSrcLayers,
                  VkOffset2D { cSrcOffset.x,     cSrcOffset.y      },
                  VkExtent2D { cSrcExtent.width, cSrcExtent.height },
                  cDstFormat);
              }
            });
          } else {
            // The backend is not aware of image metadata in this case,
            // so we need to handle image planes and block sizes here
            VkDeviceSize elementSize = dstFormatInfo->elementSize;
            VkExtent3D dstBlockSize = dstFormatInfo->blockSize;
            VkExtent3D srcBlockSize = srcFormatInfo->blockSize;
            VkExtent3D planeBlockSize = { 1u, 1u, 1u };

            if (planeCount > 1) {
              auto plane = &dstFormatInfo->planes[j];
              dstBlockSize.width  *= plane->blockSize.width;
              dstBlockSize.height *= plane->blockSize.height;
              srcBlockSize.width  *= plane->blockSize.width;
              srcBlockSize.height *= plane->blockSize.height;

              planeBlockSize.width  = plane->blockSize.width;
              planeBlockSize.height = plane->blockSize.height;
              elementSize = plane->elementSize;
            }

            EmitCs([
              cPixelSize = elementSize,
              cSrcBuffer = pSrcTexture->GetMappedBuffer(srcSubresource),
              cSrcStart  = pSrcTexture->GetSubresourceLayout(srcAspectMask, srcSubresource).Offset,
              cSrcOffset = util::computeBlockOffset(SrcOffset, srcBlockSize),
              cSrcSize   = util::computeBlockCount(srcMipExtent, srcBlockSize),
              cDstBuffer = pDstTexture->GetMappedBuffer(dstSubresource),
              cDstStart  = pDstTexture->GetSubresourceLayout(dstAspectMask, dstSubresource).Offset,
              cDstOffset = util::computeBlockOffset(DstOffset, dstBlockSize),
              cDstSize   = util::computeBlockCount(dstMipExtent, dstBlockSize),
              cExtent    = util::computeBlockCount(blockCount, planeBlockSize)
            ] (DxvkContext* ctx) {
              ctx->copyPackedBufferImage(
                cDstBuffer, cDstStart, cDstOffset, cDstSize,
                cSrcBuffer, cSrcStart, cSrcOffset, cSrcSize,
                cExtent, cPixelSize);
            });
          }
        }
      }
    }

    if (pDstTexture->HasSequenceNumber()) {
      for (uint32_t i = 0; i < pDstLayers->layerCount; i++) {
        GetTypedContext()->TrackTextureSequenceNumber(pDstTexture, D3D11CalcSubresource(
          pDstLayers->mipLevel, pDstLayers->baseArrayLayer + i, pDstTexture->Desc()->MipLevels));
      }
    }

    if (pSrcTexture->HasSequenceNumber()) {
      for (uint32_t i = 0; i < pSrcLayers->layerCount; i++) {
        GetTypedContext()->TrackTextureSequenceNumber(pSrcTexture, D3D11CalcSubresource(
          pSrcLayers->mipLevel, pSrcLayers->baseArrayLayer + i, pSrcTexture->Desc()->MipLevels));
      }
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::CopyTiledResourceData(
          ID3D11Resource*                   pResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pRegionCoordinate,
    const D3D11_TILE_REGION_SIZE*           pRegionSize,
          DxvkBufferSlice                   BufferSlice,
          UINT                              Flags) {
    Rc<DxvkPagedResource> resource = GetPagedResource(pResource);

    // Do some validation based on page table properties
    auto pageTable = resource->getSparsePageTable();

    if (!pageTable)
      return;

    if (pRegionSize->bUseBox && pRegionSize->NumTiles !=
        pRegionSize->Width * pRegionSize->Height * pRegionSize->Depth)
      return;

    if (pRegionSize->NumTiles > pageTable->getPageCount())
      return;

    // Ignore call if buffer access would be out of bounds
    VkDeviceSize bufferSize = pRegionSize->NumTiles * SparseMemoryPageSize;

    if (BufferSlice.length() < bufferSize)
      return;

    // Compute list of tile indices to copy
    std::vector<uint32_t> tiles(pRegionSize->NumTiles);

    for (uint32_t i = 0; i < pRegionSize->NumTiles; i++) {
      VkOffset3D regionOffset = {
        int32_t(pRegionCoordinate->X),
        int32_t(pRegionCoordinate->Y),
        int32_t(pRegionCoordinate->Z) };

      VkExtent3D regionExtent = {
        uint32_t(pRegionSize->Width),
        uint32_t(pRegionSize->Height),
        uint32_t(pRegionSize->Depth) };

      uint32_t tile = pageTable->computePageIndex(
        pRegionCoordinate->Subresource, regionOffset,
        regionExtent, !pRegionSize->bUseBox, i);

      // Check that the tile is valid and not part of the mip tail
      auto tileInfo = pageTable->getPageInfo(tile);

      if (tileInfo.type != DxvkSparsePageType::Buffer
       && tileInfo.type != DxvkSparsePageType::Image)
        return;

      tiles[i] = tile;
    }

    // If D3D12 is anything to go by, not passing this flag will trigger
    // the other code path, regardless of whether TO_LINEAR_BUFFER is set.
    if (Flags & D3D11_TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE) {
      EmitCs([
        cResource = std::move(resource),
        cTiles    = std::move(tiles),
        cBuffer   = std::move(BufferSlice)
      ] (DxvkContext* ctx) {
        ctx->copySparsePagesFromBuffer(
          cResource,
          cTiles.size(),
          cTiles.data(),
          cBuffer.buffer(),
          cBuffer.offset());
      });
    } else {
      EmitCs([
        cResource = std::move(resource),
        cTiles    = std::move(tiles),
        cBuffer   = std::move(BufferSlice)
      ] (DxvkContext* ctx) {
        ctx->copySparsePagesToBuffer(
          cBuffer.buffer(),
          cBuffer.offset(),
          cResource,
          cTiles.size(),
          cTiles.data());
      });
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::DiscardBuffer(
          ID3D11Resource*                   pResource) {
    auto buffer = static_cast<D3D11Buffer*>(pResource);

    if (buffer->GetMapMode() != D3D11_COMMON_BUFFER_MAP_MODE_NONE) {
      D3D11_MAPPED_SUBRESOURCE sr;

      Map(pResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
      Unmap(pResource, 0);
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::DiscardTexture(
          ID3D11Resource*                   pResource,
          UINT                              Subresource) {
    auto texture = GetCommonTexture(pResource);

    if (texture->GetMapMode() != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      D3D11_MAPPED_SUBRESOURCE sr;

      Map(pResource, Subresource, D3D11_MAP_WRITE_DISCARD, 0, &sr);
      Unmap(pResource, Subresource);
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::GetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant,
          UINT*                             pNumConstants) {
    const auto& bindings = m_state.cbv[ShaderStage];

    for (uint32_t i = 0; i < NumBuffers; i++) {
      const bool inRange = StartSlot + i < bindings.buffers.size();

      if (ppConstantBuffers) {
        ppConstantBuffers[i] = inRange
          ? bindings.buffers[StartSlot + i].buffer.ref()
          : nullptr;
      }

      if (pFirstConstant) {
        pFirstConstant[i] = inRange
          ? bindings.buffers[StartSlot + i].constantOffset
          : 0u;
      }

      if (pNumConstants) {
        pNumConstants[i] = inRange
          ? bindings.buffers[StartSlot + i].constantCount
          : 0u;
      }
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::GetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    const auto& bindings = m_state.srv[ShaderStage];

    for (uint32_t i = 0; i < NumViews; i++) {
      ppShaderResourceViews[i] = StartSlot + i < bindings.views.size()
        ? bindings.views[StartSlot + i].ref()
        : nullptr;
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::GetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    const auto& bindings = m_state.samplers[ShaderStage];

    for (uint32_t i = 0; i < NumSamplers; i++) {
      ppSamplers[i] = StartSlot + i < bindings.samplers.size()
        ? ref(bindings.samplers[StartSlot + i])
        : nullptr;
    }
  }


  template<typename ContextType>
  DxvkGlobalPipelineBarrier D3D11CommonContext<ContextType>::GetTiledResourceDependency(
          ID3D11DeviceChild*                pObject) {
    if (!pObject) {
      DxvkGlobalPipelineBarrier result;
      result.stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      result.access = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
      return result;
    } else {
      Com<ID3D11Resource> resource;

      if (FAILED(pObject->QueryInterface(IID_PPV_ARGS(&resource)))) {
        Com<ID3D11View> view;

        if (FAILED(pObject->QueryInterface(IID_PPV_ARGS(&view))))
          return DxvkGlobalPipelineBarrier();

        view->GetResource(&resource);
      }

      D3D11CommonTexture* texture = GetCommonTexture(resource.ptr());

      if (texture) {
        Rc<DxvkImage> image = texture->GetImage();

        DxvkGlobalPipelineBarrier result;
        result.stages = image->info().stages;
        result.access = image->info().access;
        return result;
      } else {
        Rc<DxvkBuffer> buffer = static_cast<D3D11Buffer*>(resource.ptr())->GetBuffer();

        if (buffer == nullptr)
          return DxvkGlobalPipelineBarrier();

        DxvkGlobalPipelineBarrier result;
        result.stages = buffer->info().stages;
        result.access = buffer->info().access;
        return result;
      }
    }
  }


  template<typename ContextType>
  D3D11MaxUsedBindings D3D11CommonContext<ContextType>::GetMaxUsedBindings() {
    D3D11MaxUsedBindings result;

    for (uint32_t i = 0; i < result.stages.size(); i++) {
      auto stage = DxbcProgramType(i);

      result.stages[i].cbvCount = m_state.cbv[stage].maxCount;
      result.stages[i].srvCount = m_state.srv[stage].maxCount;
      result.stages[i].uavCount = 0;
      result.stages[i].samplerCount = m_state.samplers[stage].maxCount;
      result.stages[i].reserved = 0;
    }

    result.stages[uint32_t(DxbcProgramType::PixelShader)].uavCount = m_state.om.maxUav;
    result.stages[uint32_t(DxbcProgramType::ComputeShader)].uavCount = m_state.uav.maxCount;

    result.vbCount = m_state.ia.maxVbCount;
    result.soCount = D3D11_SO_BUFFER_SLOT_COUNT;
    return result;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ResetCommandListState() {
    EmitCs([
      cUsedBindings = GetMaxUsedBindings()
    ] (DxvkContext* ctx) {
      // Reset render targets
      ctx->bindRenderTargets(DxvkRenderTargets(), 0u);

      // Reset vertex input state
      ctx->setInputLayout(0, nullptr, 0, nullptr);

      // Reset render states
      DxvkInputAssemblyState iaState;
      InitDefaultPrimitiveTopology(&iaState);

      DxvkDepthStencilState dsState;
      InitDefaultDepthStencilState(&dsState);

      DxvkRasterizerState rsState;
      InitDefaultRasterizerState(&rsState);

      DxvkBlendMode cbState;
      DxvkLogicOpState loState;
      DxvkMultisampleState msState;
      InitDefaultBlendState(&cbState, &loState, &msState, D3D11_DEFAULT_SAMPLE_MASK);

      ctx->setInputAssemblyState(iaState);
      ctx->setDepthStencilState(dsState);
      ctx->setRasterizerState(rsState);
      ctx->setLogicOpState(loState);
      ctx->setMultisampleState(msState);

      for (uint32_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        ctx->setBlendMode(i, cbState);

      // Reset dynamic states
      ctx->setBlendConstants(DxvkBlendConstants { 1.0f, 1.0f, 1.0f, 1.0f });
      ctx->setStencilReference(D3D11_DEFAULT_STENCIL_REFERENCE);

      // Reset viewports
      auto viewport = VkViewport();
      auto scissor  = VkRect2D();

      ctx->setViewports(1, &viewport, &scissor);

      // Unbind indirect draw buffer
      ctx->bindDrawBuffers(DxvkBufferSlice(), DxvkBufferSlice());

      // Unbind index and vertex buffers
      ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);

      for (uint32_t i = 0; i < cUsedBindings.vbCount; i++)
        ctx->bindVertexBuffer(i, DxvkBufferSlice(), 0);

      // Unbind transform feedback buffers
      for (uint32_t i = 0; i < cUsedBindings.soCount; i++)
        ctx->bindXfbBuffer(i, DxvkBufferSlice(), DxvkBufferSlice());

      // Unbind all shaders
      ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_GEOMETRY_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_COMPUTE_BIT>(nullptr);

      // Unbind per-shader stage resources
      for (uint32_t i = 0; i < 6; i++) {
        auto programType = DxbcProgramType(i);
        auto stage = GetShaderStage(programType);

        // Unbind constant buffers, including the shader's ICB
        auto cbSlotId = computeConstantBufferBinding(programType, 0);
        ctx->bindUniformBuffer(stage, cbSlotId + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, DxvkBufferSlice());

        for (uint32_t j = 0; j < cUsedBindings.stages[i].cbvCount; j++)
          ctx->bindUniformBuffer(stage, cbSlotId + j, DxvkBufferSlice());

        // Unbind shader resource views
        auto srvSlotId = computeSrvBinding(programType, 0);

        for (uint32_t j = 0; j < cUsedBindings.stages[i].srvCount; j++)
          ctx->bindResourceImageView(stage, srvSlotId + j, nullptr);

        // Unbind texture samplers
        auto samplerSlotId = computeSamplerBinding(programType, 0);

        for (uint32_t j = 0; j < cUsedBindings.stages[i].samplerCount; j++)
          ctx->bindResourceSampler(stage, samplerSlotId + j, nullptr);

        // Unbind UAVs for supported stages
        if (programType == DxbcProgramType::PixelShader
         || programType == DxbcProgramType::ComputeShader) {
          VkShaderStageFlags stages = programType == DxbcProgramType::PixelShader
            ? VK_SHADER_STAGE_ALL_GRAPHICS
            : VK_SHADER_STAGE_COMPUTE_BIT;

          auto uavSlotId = computeUavBinding(programType, 0);
          auto ctrSlotId = computeUavCounterBinding(programType, 0);

          for (uint32_t j = 0; j < cUsedBindings.stages[i].uavCount; j++) {
            ctx->bindResourceImageView(stages, uavSlotId, nullptr);
            ctx->bindResourceBufferView(stages, ctrSlotId, nullptr);
          }
        }
      }

      // Initialize push constants
      DxbcPushConstants pc;
      pc.rasterizerSampleCount = 1;
      ctx->pushConstants(0, sizeof(pc), &pc);
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ResetContextState() {
    // Reset shaders
    m_state.vs = nullptr;
    m_state.hs = nullptr;
    m_state.ds = nullptr;
    m_state.gs = nullptr;
    m_state.ps = nullptr;
    m_state.cs = nullptr;

    // Reset render state
    m_state.id.reset();
    m_state.ia.reset();
    m_state.om.reset();
    m_state.rs.reset();
    m_state.so.reset();
    m_state.pr.reset();

    // Reset resource bindings
    m_state.cbv.reset();
    m_state.srv.reset();
    m_state.uav.reset();
    m_state.samplers.reset();
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ResetStagingBuffer() {
    m_staging.reset();
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage, typename T>
  void D3D11CommonContext<ContextType>::ResolveSrvHazards(
          T*                                pView) {
    auto& bindings = m_state.srv[ShaderStage];

    uint32_t slotId = computeSrvBinding(ShaderStage, 0);
    int32_t srvId = bindings.hazardous.findNext(0);

    while (srvId >= 0) {
      auto srv = bindings.views[srvId].ptr();

      if (likely(srv && srv->TestHazards())) {
        bool hazard = CheckViewOverlap(pView, srv);

        if (unlikely(hazard)) {
          bindings.views[srvId] = nullptr;
          bindings.hazardous.clr(srvId);

          BindShaderResource<ShaderStage>(slotId + srvId, nullptr);
        }
      } else {
        // Avoid further redundant iterations
        bindings.hazardous.clr(srvId);
      }

      srvId = bindings.hazardous.findNext(srvId + 1);
    }
  }


  template<typename ContextType>
  template<typename T>
  void D3D11CommonContext<ContextType>::ResolveCsSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::ComputeShader>(pView);
  }


  template<typename ContextType>
  template<typename T>
  void D3D11CommonContext<ContextType>::ResolveOmSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::VertexShader>(pView);
    ResolveSrvHazards<DxbcProgramType::HullShader>(pView);
    ResolveSrvHazards<DxbcProgramType::DomainShader>(pView);
    ResolveSrvHazards<DxbcProgramType::GeometryShader>(pView);
    ResolveSrvHazards<DxbcProgramType::PixelShader>(pView);
  }


  template<typename ContextType>
  bool D3D11CommonContext<ContextType>::ResolveOmRtvHazards(
          D3D11UnorderedAccessView*         pView) {
    if (!pView || !pView->HasBindFlag(D3D11_BIND_RENDER_TARGET))
      return false;

    bool hazard = false;

    if (CheckViewOverlap(pView, m_state.om.dsv.ptr())) {
      m_state.om.dsv = nullptr;
      hazard = true;
    }

    for (uint32_t i = 0; i < m_state.om.maxRtv; i++) {
      if (CheckViewOverlap(pView, m_state.om.rtvs[i].ptr())) {
        m_state.om.rtvs[i] = nullptr;
        hazard = true;
      }
    }

    return hazard;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ResolveOmUavHazards(
          D3D11RenderTargetView*            pView) {
    if (!pView || !pView->HasBindFlag(D3D11_BIND_UNORDERED_ACCESS))
      return;

    uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::PixelShader, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::PixelShader, 0);

    for (uint32_t i = 0; i < m_state.om.maxUav; i++) {
      if (CheckViewOverlap(pView, m_state.om.uavs[i].ptr())) {
        m_state.om.uavs[i] = nullptr;

        BindUnorderedAccessView<DxbcProgramType::PixelShader>(
          uavSlotId + i, nullptr,
          ctrSlotId + i, ~0u);
      }
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::RestoreCommandListState() {
    BindFramebuffer();

    BindShader<DxbcProgramType::VertexShader>(GetCommonShader(m_state.vs.ptr()));
    BindShader<DxbcProgramType::HullShader>(GetCommonShader(m_state.hs.ptr()));
    BindShader<DxbcProgramType::DomainShader>(GetCommonShader(m_state.ds.ptr()));
    BindShader<DxbcProgramType::GeometryShader>(GetCommonShader(m_state.gs.ptr()));
    BindShader<DxbcProgramType::PixelShader>(GetCommonShader(m_state.ps.ptr()));
    BindShader<DxbcProgramType::ComputeShader>(GetCommonShader(m_state.cs.ptr()));

    ApplyInputLayout();
    ApplyPrimitiveTopology();
    ApplyBlendState();
    ApplyBlendFactor();
    ApplyDepthStencilState();
    ApplyStencilRef();
    ApplyRasterizerState();
    ApplyRasterizerSampleCount();
    ApplyViewportState();

    BindDrawBuffers(
      m_state.id.argBuffer.ptr(),
      m_state.id.cntBuffer.ptr());

    BindIndexBuffer(
      m_state.ia.indexBuffer.buffer.ptr(),
      m_state.ia.indexBuffer.offset,
      m_state.ia.indexBuffer.format);

    for (uint32_t i = 0; i < m_state.ia.maxVbCount; i++) {
      BindVertexBuffer(i,
        m_state.ia.vertexBuffers[i].buffer.ptr(),
        m_state.ia.vertexBuffers[i].offset,
        m_state.ia.vertexBuffers[i].stride);
    }

    for (uint32_t i = 0; i < m_state.so.targets.size(); i++)
      BindXfbBuffer(i, m_state.so.targets[i].buffer.ptr(), ~0u);

    RestoreConstantBuffers<DxbcProgramType::VertexShader>();
    RestoreConstantBuffers<DxbcProgramType::HullShader>();
    RestoreConstantBuffers<DxbcProgramType::DomainShader>();
    RestoreConstantBuffers<DxbcProgramType::GeometryShader>();
    RestoreConstantBuffers<DxbcProgramType::PixelShader>();
    RestoreConstantBuffers<DxbcProgramType::ComputeShader>();

    RestoreShaderResources<DxbcProgramType::VertexShader>();
    RestoreShaderResources<DxbcProgramType::HullShader>();
    RestoreShaderResources<DxbcProgramType::DomainShader>();
    RestoreShaderResources<DxbcProgramType::GeometryShader>();
    RestoreShaderResources<DxbcProgramType::PixelShader>();
    RestoreShaderResources<DxbcProgramType::ComputeShader>();

    RestoreUnorderedAccessViews<DxbcProgramType::PixelShader>();
    RestoreUnorderedAccessViews<DxbcProgramType::ComputeShader>();

    RestoreSamplers<DxbcProgramType::VertexShader>();
    RestoreSamplers<DxbcProgramType::HullShader>();
    RestoreSamplers<DxbcProgramType::DomainShader>();
    RestoreSamplers<DxbcProgramType::GeometryShader>();
    RestoreSamplers<DxbcProgramType::PixelShader>();
    RestoreSamplers<DxbcProgramType::ComputeShader>();
  }


  template<typename ContextType>
  template<DxbcProgramType Stage>
  void D3D11CommonContext<ContextType>::RestoreConstantBuffers() {
    const auto& bindings = m_state.cbv[Stage];
    uint32_t slotId = computeConstantBufferBinding(Stage, 0);

    for (uint32_t i = 0; i < bindings.maxCount; i++) {
      BindConstantBuffer<Stage>(slotId + i, bindings.buffers[i].buffer.ptr(),
        bindings.buffers[i].constantOffset, bindings.buffers[i].constantBound);
    }
  }


  template<typename ContextType>
  template<DxbcProgramType Stage>
  void D3D11CommonContext<ContextType>::RestoreSamplers() {
    const auto& bindings = m_state.samplers[Stage];
    uint32_t slotId = computeSamplerBinding(Stage, 0);

    for (uint32_t i = 0; i < bindings.maxCount; i++)
      BindSampler<Stage>(slotId + i, bindings.samplers[i]);
  }


  template<typename ContextType>
  template<DxbcProgramType Stage>
  void D3D11CommonContext<ContextType>::RestoreShaderResources() {
    const auto& bindings = m_state.srv[Stage];
    uint32_t slotId = computeSrvBinding(Stage, 0);

    for (uint32_t i = 0; i < bindings.maxCount; i++)
      BindShaderResource<Stage>(slotId + i, bindings.views[i].ptr());
  }


  template<typename ContextType>
  template<DxbcProgramType Stage>
  void D3D11CommonContext<ContextType>::RestoreUnorderedAccessViews() {
    const auto& views = Stage == DxbcProgramType::ComputeShader
      ? m_state.uav.views
      : m_state.om.uavs;

    uint32_t maxCount = Stage == DxbcProgramType::ComputeShader
      ? m_state.uav.maxCount
      : m_state.om.maxUav;

    uint32_t uavSlotId = computeUavBinding(Stage, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(Stage, 0);

    for (uint32_t i = 0; i < maxCount; i++) {
      BindUnorderedAccessView<Stage>(
        uavSlotId + i, views[i].ptr(),
        ctrSlotId + i, ~0u);
    }
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::SetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    auto& bindings = m_state.cbv[ShaderStage];
    uint32_t slotId = computeConstantBufferBinding(ShaderStage, StartSlot);

    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);

      uint32_t constantCount = newBuffer
        ? std::min(newBuffer->Desc()->ByteWidth / 16, UINT(D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT))
        : 0u;

      if (bindings.buffers[StartSlot + i].buffer         != newBuffer
       || bindings.buffers[StartSlot + i].constantOffset != 0
       || bindings.buffers[StartSlot + i].constantCount  != constantCount) {
        bindings.buffers[StartSlot + i].buffer         = newBuffer;
        bindings.buffers[StartSlot + i].constantOffset = 0;
        bindings.buffers[StartSlot + i].constantCount  = constantCount;
        bindings.buffers[StartSlot + i].constantBound  = constantCount;

        BindConstantBuffer<ShaderStage>(slotId + i, newBuffer, 0, constantCount);
      }
    }

    bindings.maxCount = std::clamp(StartSlot + NumBuffers,
      bindings.maxCount, uint32_t(bindings.buffers.size()));
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::SetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    auto& bindings = m_state.cbv[ShaderStage];

    uint32_t slotId = computeConstantBufferBinding(ShaderStage, StartSlot);

    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);

      UINT constantOffset;
      UINT constantCount;
      UINT constantBound;

      if (likely(newBuffer != nullptr)) {
        UINT bufferConstantsCount = newBuffer->Desc()->ByteWidth / 16;
        constantBound = std::min(bufferConstantsCount, UINT(D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT));

        if (likely(pFirstConstant && pNumConstants)) {
          constantOffset  = pFirstConstant[i];
          constantCount   = pNumConstants [i];

          if (unlikely(constantCount > D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT))
            continue;

          constantBound = (constantOffset + constantCount > bufferConstantsCount)
            ? bufferConstantsCount - std::min(constantOffset, bufferConstantsCount)
            : constantCount;
        } else {
          constantOffset  = 0;
          constantCount   = constantBound;
        }
      } else {
        constantOffset  = 0;
        constantCount   = 0;
        constantBound   = 0;
      }

      // Do a full rebind if either the buffer changes
      if (bindings.buffers[StartSlot + i].buffer != newBuffer) {
        bindings.buffers[StartSlot + i].buffer = newBuffer;
        bindings.buffers[StartSlot + i].constantOffset = constantOffset;
        bindings.buffers[StartSlot + i].constantCount  = constantCount;
        bindings.buffers[StartSlot + i].constantBound  = constantBound;

        BindConstantBuffer<ShaderStage>(slotId + i, newBuffer, constantOffset, constantBound);
      } else if (bindings.buffers[StartSlot + i].constantOffset != constantOffset
              || bindings.buffers[StartSlot + i].constantCount  != constantCount) {
        bindings.buffers[StartSlot + i].constantOffset = constantOffset;
        bindings.buffers[StartSlot + i].constantCount  = constantCount;
        bindings.buffers[StartSlot + i].constantBound  = constantBound;

        BindConstantBufferRange<ShaderStage>(slotId + i, constantOffset, constantBound);
      }
    }

    bindings.maxCount = std::clamp(StartSlot + NumBuffers,
      bindings.maxCount, uint32_t(bindings.buffers.size()));
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::SetShaderResources(
          UINT                              StartSlot,
          UINT                              NumResources,
          ID3D11ShaderResourceView* const*  ppResources) {
    auto& bindings = m_state.srv[ShaderStage];
    uint32_t slotId = computeSrvBinding(ShaderStage, StartSlot);

    for (uint32_t i = 0; i < NumResources; i++) {
      auto resView = static_cast<D3D11ShaderResourceView*>(ppResources[i]);

      if (bindings.views[StartSlot + i] != resView) {
        if (likely(resView != nullptr)) {
          if (unlikely(resView->TestHazards())) {
            if (TestSrvHazards<ShaderStage>(resView))
              resView = nullptr;

            // Only set if necessary, but don't reset it on every
            // bind as this would be more expensive than a few
            // redundant checks in OMSetRenderTargets and friends.
            bindings.hazardous.set(StartSlot + i, resView);
          }
        }

        bindings.views[StartSlot + i] = resView;
        BindShaderResource<ShaderStage>(slotId + i, resView);
      }
    }

    bindings.maxCount = std::clamp(StartSlot + NumResources,
      bindings.maxCount, uint32_t(bindings.views.size()));
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::SetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    auto& bindings = m_state.samplers[ShaderStage];
    uint32_t slotId = computeSamplerBinding(ShaderStage, StartSlot);

    for (uint32_t i = 0; i < NumSamplers; i++) {
      auto sampler = static_cast<D3D11SamplerState*>(ppSamplers[i]);

      if (bindings.samplers[StartSlot + i] != sampler) {
        bindings.samplers[StartSlot + i] = sampler;
        BindSampler<ShaderStage>(slotId + i, sampler);
      }
    }

    bindings.maxCount = std::clamp(StartSlot + NumSamplers,
      bindings.maxCount, uint32_t(bindings.samplers.size()));
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::SetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    if (TestRtvUavHazards(NumRTVs, ppRenderTargetViews, NumUAVs, ppUnorderedAccessViews))
      return;

    bool needsUpdate = false;

    if (likely(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)) {
      // Native D3D11 does not change the render targets if
      // the parameters passed to this method are invalid.
      if (!ValidateRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView))
        return;

      for (uint32_t i = 0; i < m_state.om.rtvs.size(); i++) {
        auto rtv = i < NumRTVs
          ? static_cast<D3D11RenderTargetView*>(ppRenderTargetViews[i])
          : nullptr;

        if (m_state.om.rtvs[i] != rtv) {
          m_state.om.rtvs[i] = rtv;
          needsUpdate = true;
          ResolveOmSrvHazards(rtv);

          if (NumUAVs == D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
            ResolveOmUavHazards(rtv);
        }
      }

      auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);

      if (m_state.om.dsv != dsv) {
        m_state.om.dsv = dsv;
        needsUpdate = true;
        ResolveOmSrvHazards(dsv);
      }

      m_state.om.maxRtv = NumRTVs;
    }

    if (unlikely(NumUAVs || m_state.om.maxUav)) {
      uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::PixelShader, 0);
      uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::PixelShader, 0);

      if (likely(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)) {
        uint32_t newMaxUav = NumUAVs ? UAVStartSlot + NumUAVs : 0;
        uint32_t oldMaxUav = std::exchange(m_state.om.maxUav, newMaxUav);

        for (uint32_t i = 0; i < std::max(oldMaxUav, newMaxUav); i++) {
          D3D11UnorderedAccessView* uav = nullptr;
          uint32_t                  ctr = ~0u;

          if (i >= UAVStartSlot && i < UAVStartSlot + NumUAVs) {
            uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i - UAVStartSlot]);
            ctr = pUAVInitialCounts ? pUAVInitialCounts[i - UAVStartSlot] : ~0u;
          }

          if (m_state.om.uavs[i] != uav || ctr != ~0u) {
            m_state.om.uavs[i] = uav;

            BindUnorderedAccessView<DxbcProgramType::PixelShader>(
              uavSlotId + i, uav,
              ctrSlotId + i, ctr);

            ResolveOmSrvHazards(uav);

            if (NumRTVs == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
              needsUpdate |= ResolveOmRtvHazards(uav);
          }
        }
      }
    }

    if (needsUpdate) {
      BindFramebuffer();

      if constexpr (!IsDeferred) {
        // Doing this makes it less likely to flush during render passes
        GetTypedContext()->ConsiderFlush(GpuFlushType::ImplicitWeakHint);
      }
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::SetDrawBuffers(
          ID3D11Buffer*                     pBufferForArgs,
          ID3D11Buffer*                     pBufferForCount) {
    auto argBuffer = static_cast<D3D11Buffer*>(pBufferForArgs);
    auto cntBuffer = static_cast<D3D11Buffer*>(pBufferForCount);

    if (m_state.id.argBuffer != argBuffer
     || m_state.id.cntBuffer != cntBuffer) {
      m_state.id.argBuffer = argBuffer;
      m_state.id.cntBuffer = cntBuffer;

      BindDrawBuffers(argBuffer, cntBuffer);
    }
  }


  template<typename ContextType>
  bool D3D11CommonContext<ContextType>::TestRtvUavHazards(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRTVs,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUAVs) {
    if (NumRTVs == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) NumRTVs = 0;
    if (NumUAVs == D3D11_KEEP_UNORDERED_ACCESS_VIEWS)           NumUAVs = 0;

    for (uint32_t i = 0; i < NumRTVs; i++) {
      auto rtv = static_cast<D3D11RenderTargetView*>(ppRTVs[i]);

      if (!rtv)
        continue;

      for (uint32_t j = 0; j < i; j++) {
        if (CheckViewOverlap(rtv, static_cast<D3D11RenderTargetView*>(ppRTVs[j])))
          return true;
      }

      if (rtv->HasBindFlag(D3D11_BIND_UNORDERED_ACCESS)) {
        for (uint32_t j = 0; j < NumUAVs; j++) {
          if (CheckViewOverlap(rtv, static_cast<D3D11UnorderedAccessView*>(ppUAVs[j])))
            return true;
        }
      }
    }

    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUAVs[i]);

      if (!uav)
        continue;

      for (uint32_t j = 0; j < i; j++) {
        if (CheckViewOverlap(uav, static_cast<D3D11UnorderedAccessView*>(ppUAVs[j])))
          return true;
      }
    }

    return false;
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  bool D3D11CommonContext<ContextType>::TestSrvHazards(
          D3D11ShaderResourceView*          pView) {
    bool hazard = false;

    if (ShaderStage == DxbcProgramType::ComputeShader) {
      int32_t uav = m_state.uav.mask.findNext(0);

      while (uav >= 0 && !hazard) {
        hazard = CheckViewOverlap(pView, m_state.uav.views[uav].ptr());
        uav = m_state.uav.mask.findNext(uav + 1);
      }
    } else {
      hazard = CheckViewOverlap(pView, m_state.om.dsv.ptr());

      for (uint32_t i = 0; !hazard && i < m_state.om.maxRtv; i++)
        hazard = CheckViewOverlap(pView, m_state.om.rtvs[i].ptr());

      for (uint32_t i = 0; !hazard && i < m_state.om.maxUav; i++)
        hazard = CheckViewOverlap(pView, m_state.om.uavs[i].ptr());
    }

    return hazard;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::TrackResourceSequenceNumber(
          ID3D11Resource*             pResource) {
    if (!pResource)
      return;

    D3D11CommonTexture* texture = GetCommonTexture(pResource);

    if (texture) {
      if (texture->HasSequenceNumber()) {
        for (uint32_t i = 0; i < texture->CountSubresources(); i++)
          GetTypedContext()->TrackTextureSequenceNumber(texture, i);
      }
    } else {
      D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pResource);

      if (buffer->HasSequenceNumber())
        GetTypedContext()->TrackBufferSequenceNumber(buffer);
    }
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::UpdateBuffer(
          D3D11Buffer*                      pDstBuffer,
          UINT                              Offset,
          UINT                              Length,
    const void*                             pSrcData) {
    DxvkBufferSlice bufferSlice = pDstBuffer->GetBufferSlice(Offset, Length);

    if (Length <= 1024 && !(Offset & 0x3) && !(Length & 0x3)) {
      // The backend has special code paths for small buffer updates,
      // however both offset and size must be aligned to four bytes.
      DxvkDataSlice dataSlice = AllocUpdateBufferSlice(Length);
      std::memcpy(dataSlice.ptr(), pSrcData, Length);

      EmitCs([
        cDataBuffer   = std::move(dataSlice),
        cBufferSlice  = std::move(bufferSlice)
      ] (DxvkContext* ctx) {
        ctx->updateBuffer(
          cBufferSlice.buffer(),
          cBufferSlice.offset(),
          cBufferSlice.length(),
          cDataBuffer.ptr());
      });
    } else {
      // Otherwise, to avoid large data copies on the CS thread,
      // write directly to a staging buffer and dispatch a copy
      DxvkBufferSlice stagingSlice = AllocStagingBuffer(Length);
      std::memcpy(stagingSlice.mapPtr(0), pSrcData, Length);

      EmitCs([
        cStagingSlice = std::move(stagingSlice),
        cBufferSlice  = std::move(bufferSlice)
      ] (DxvkContext* ctx) {
        ctx->copyBuffer(
          cBufferSlice.buffer(),
          cBufferSlice.offset(),
          cStagingSlice.buffer(),
          cStagingSlice.offset(),
          cBufferSlice.length());
      });
    }

    if (pDstBuffer->HasSequenceNumber())
      GetTypedContext()->TrackBufferSequenceNumber(pDstBuffer);
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::UpdateTexture(
          D3D11CommonTexture*               pDstTexture,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    if (DstSubresource >= pDstTexture->CountSubresources())
      return;

    VkFormat packedFormat = pDstTexture->GetPackedFormat();

    auto formatInfo = lookupFormatInfo(packedFormat);
    auto subresource = pDstTexture->GetSubresourceFromIndex(
        formatInfo->aspectMask, DstSubresource);

    VkExtent3D mipExtent = pDstTexture->MipLevelExtent(subresource.mipLevel);

    VkOffset3D offset = { 0, 0, 0 };
    VkExtent3D extent = mipExtent;

    if (pDstBox != nullptr) {
      if (pDstBox->left >= pDstBox->right
        || pDstBox->top >= pDstBox->bottom
        || pDstBox->front >= pDstBox->back)
        return;  // no-op, but legal

      offset.x = pDstBox->left;
      offset.y = pDstBox->top;
      offset.z = pDstBox->front;

      extent.width  = pDstBox->right - pDstBox->left;
      extent.height = pDstBox->bottom - pDstBox->top;
      extent.depth  = pDstBox->back - pDstBox->front;
    }

    if (!util::isBlockAligned(offset, extent, formatInfo->blockSize, mipExtent))
      return;

    auto stagingSlice = AllocStagingBuffer(util::computeImageDataSize(packedFormat, extent));

    util::packImageData(stagingSlice.mapPtr(0),
      pSrcData, SrcRowPitch, SrcDepthPitch, 0, 0,
      pDstTexture->GetVkImageType(), extent, 1,
      formatInfo, formatInfo->aspectMask);

    UpdateImage(pDstTexture, &subresource,
      offset, extent, std::move(stagingSlice));
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::UpdateImage(
          D3D11CommonTexture*               pDstTexture,
    const VkImageSubresource*               pDstSubresource,
          VkOffset3D                        DstOffset,
          VkExtent3D                        DstExtent,
          DxvkBufferSlice                   StagingBuffer) {
    bool dstIsImage = pDstTexture->GetMapMode() != D3D11_COMMON_TEXTURE_MAP_MODE_STAGING;

    uint32_t dstSubresource = D3D11CalcSubresource(pDstSubresource->mipLevel,
      pDstSubresource->arrayLayer, pDstTexture->Desc()->MipLevels);

    if (dstIsImage) {
      EmitCs([
        cDstImage         = pDstTexture->GetImage(),
        cDstLayers        = vk::makeSubresourceLayers(*pDstSubresource),
        cDstOffset        = DstOffset,
        cDstExtent        = DstExtent,
        cStagingSlice     = std::move(StagingBuffer),
        cPackedFormat     = pDstTexture->GetPackedFormat()
      ] (DxvkContext* ctx) {
        if (cDstLayers.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
          ctx->copyBufferToImage(cDstImage,
            cDstLayers, cDstOffset, cDstExtent,
            cStagingSlice.buffer(),
            cStagingSlice.offset(), 0, 0);
        } else {
          ctx->copyPackedBufferToDepthStencilImage(cDstImage, cDstLayers,
            VkOffset2D { cDstOffset.x,     cDstOffset.y      },
            VkExtent2D { cDstExtent.width, cDstExtent.height },
            cStagingSlice.buffer(),
            cStagingSlice.offset(),
            VkOffset2D { 0, 0 },
            VkExtent2D { cDstExtent.width, cDstExtent.height },
            cPackedFormat);
        }
      });
    } else {
      // If the destination image is backed only by a buffer, we need to use
      // the packed buffer copy function which does not know about planes and
      // format metadata, so deal with it manually here.
      VkExtent3D dstMipExtent = pDstTexture->MipLevelExtent(pDstSubresource->mipLevel);

      auto dstFormat = pDstTexture->GetPackedFormat();
      auto dstFormatInfo = lookupFormatInfo(dstFormat);

      uint32_t planeCount = 1;

      if (dstFormatInfo->flags.test(DxvkFormatFlag::MultiPlane))
        planeCount = vk::getPlaneCount(dstFormatInfo->aspectMask);

      // The source data isn't stored in an image so we'll also need to
      // track the offset for that while iterating over the planes.
      VkDeviceSize srcPlaneOffset = 0;

      for (uint32_t i = 0; i < planeCount; i++) {
        VkImageAspectFlags dstAspectMask = dstFormatInfo->aspectMask;
        VkDeviceSize elementSize = dstFormatInfo->elementSize;
        VkExtent3D blockSize = dstFormatInfo->blockSize;

        if (dstFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
          dstAspectMask = vk::getPlaneAspect(i);

          auto plane = &dstFormatInfo->planes[i];
          blockSize.width  *= plane->blockSize.width;
          blockSize.height *= plane->blockSize.height;
          elementSize = plane->elementSize;
        }

        VkExtent3D blockCount = util::computeBlockCount(DstExtent, blockSize);

        EmitCs([
          cDstBuffer      = pDstTexture->GetMappedBuffer(dstSubresource),
          cDstStart       = pDstTexture->GetSubresourceLayout(dstAspectMask, dstSubresource).Offset,
          cDstOffset      = util::computeBlockOffset(DstOffset, blockSize),
          cDstSize        = util::computeBlockCount(dstMipExtent, blockSize),
          cDstExtent      = blockCount,
          cSrcBuffer      = StagingBuffer.buffer(),
          cSrcStart       = StagingBuffer.offset() + srcPlaneOffset,
          cPixelSize      = elementSize
        ] (DxvkContext* ctx) {
          ctx->copyPackedBufferImage(
            cDstBuffer, cDstStart, cDstOffset, cDstSize,
            cSrcBuffer, cSrcStart, VkOffset3D(), cDstExtent,
            cDstExtent, cPixelSize);
        });

        srcPlaneOffset += util::flattenImageExtent(blockCount) * elementSize;
      }
    }

    if (pDstTexture->HasSequenceNumber())
      GetTypedContext()->TrackTextureSequenceNumber(pDstTexture, dstSubresource);
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::UpdateResource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch,
          UINT                              CopyFlags) {
    auto context = static_cast<ContextType*>(this);
    D3D10DeviceLock lock = context->LockContext();

    if (!pDstResource)
      return;

    // We need a different code path for buffers
    D3D11_RESOURCE_DIMENSION resourceType;
    pDstResource->GetType(&resourceType);

    if (likely(resourceType == D3D11_RESOURCE_DIMENSION_BUFFER)) {
      const auto bufferResource = static_cast<D3D11Buffer*>(pDstResource);
      uint64_t bufferSize = bufferResource->Desc()->ByteWidth;

      // Provide a fast path for mapped buffer updates since some
      // games use UpdateSubresource to update constant buffers.
      if (likely(bufferResource->GetMapMode() == D3D11_COMMON_BUFFER_MAP_MODE_DIRECT) && likely(!pDstBox)) {
        context->UpdateMappedBuffer(bufferResource, 0, bufferSize, pSrcData, 0);
        return;
      }

      // Validate buffer range to update
      uint64_t offset = 0;
      uint64_t length = bufferSize;

      if (pDstBox) {
        offset = pDstBox->left;
        length = pDstBox->right - offset;
      }

      if (unlikely(offset + length > bufferSize))
        return;

      // Still try to be fast if a box is provided but we update the full buffer
      if (likely(bufferResource->GetMapMode() == D3D11_COMMON_BUFFER_MAP_MODE_DIRECT)) {
        CopyFlags &= D3D11_COPY_DISCARD | D3D11_COPY_NO_OVERWRITE;

        if (likely(length == bufferSize) || unlikely(CopyFlags != 0)) {
          context->UpdateMappedBuffer(bufferResource, offset, length, pSrcData, CopyFlags);
          return;
        }
      }

      // Otherwise we can't really do anything fancy, so just do a GPU copy
      context->UpdateBuffer(bufferResource, offset, length, pSrcData);
    } else {
      D3D11CommonTexture* textureResource = GetCommonTexture(pDstResource);

      context->UpdateTexture(textureResource,
        DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
    }
  }


  template<typename ContextType>
  bool D3D11CommonContext<ContextType>::ValidateRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    Rc<DxvkImageView> refView;

    VkExtent3D dsvExtent = { 0u, 0u, 0u };
    VkExtent3D rtvExtent = { 0u, 0u, 0u };

    if (pDepthStencilView != nullptr) {
      refView = static_cast<D3D11DepthStencilView*>(
        pDepthStencilView)->GetImageView();
      dsvExtent = refView->mipLevelExtent(0);
    }

    for (uint32_t i = 0; i < NumViews; i++) {
      if (ppRenderTargetViews[i] != nullptr) {
        auto curView = static_cast<D3D11RenderTargetView*>(
          ppRenderTargetViews[i])->GetImageView();

        if (!rtvExtent.width)
          rtvExtent = curView->mipLevelExtent(0);

        if (refView != nullptr) {
          // Render target views must all have the same sample count,
          // layer count, and type. The size can mismatch under certain
          // conditions, the D3D11 documentation is wrong here.
          if (curView->info().type      != refView->info().type
           || curView->info().numLayers != refView->info().numLayers)
            return false;

          if (curView->imageInfo().sampleCount
           != refView->imageInfo().sampleCount)
            return false;

          // Color targets must all be the same size
          VkExtent3D curExtent = curView->mipLevelExtent(0);

          if (curExtent.width  != rtvExtent.width
           || curExtent.height != rtvExtent.height)
            return false;
        } else {
          // Set reference view. All remaining views
          // must be compatible to the reference view.
          refView = curView;
        }
      }
    }

    // Based on testing, the depth-stencil target is allowed
    // to be larger than all color targets, but not smaller
    if (rtvExtent.width && dsvExtent.width) {
      if (rtvExtent.width  > dsvExtent.width
       || rtvExtent.height > dsvExtent.height)
        return false;
    }

    return true;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::InitDefaultPrimitiveTopology(
          DxvkInputAssemblyState*           pIaState) {
    pIaState->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    pIaState->primitiveRestart  = VK_FALSE;
    pIaState->patchVertexCount  = 0;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::InitDefaultRasterizerState(
          DxvkRasterizerState*              pRsState) {
    pRsState->polygonMode     = VK_POLYGON_MODE_FILL;
    pRsState->cullMode        = VK_CULL_MODE_BACK_BIT;
    pRsState->frontFace       = VK_FRONT_FACE_CLOCKWISE;
    pRsState->depthClipEnable = VK_TRUE;
    pRsState->depthBiasEnable = VK_FALSE;
    pRsState->conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    pRsState->sampleCount     = 0;
    pRsState->flatShading     = VK_FALSE;
    pRsState->lineMode        = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::InitDefaultDepthStencilState(
          DxvkDepthStencilState*            pDsState) {
    VkStencilOpState stencilOp;
    stencilOp.failOp            = VK_STENCIL_OP_KEEP;
    stencilOp.passOp            = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp       = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = D3D11_DEFAULT_STENCIL_READ_MASK;
    stencilOp.writeMask         = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    stencilOp.reference         = 0;

    pDsState->enableDepthTest   = VK_TRUE;
    pDsState->enableDepthWrite  = VK_TRUE;
    pDsState->enableStencilTest = VK_FALSE;
    pDsState->depthCompareOp    = VK_COMPARE_OP_LESS;
    pDsState->stencilOpFront    = stencilOp;
    pDsState->stencilOpBack     = stencilOp;
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::InitDefaultBlendState(
          DxvkBlendMode*                    pCbState,
          DxvkLogicOpState*                 pLoState,
          DxvkMultisampleState*             pMsState,
          UINT                              SampleMask) {
    pCbState->enableBlending    = VK_FALSE;
    pCbState->colorSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->colorDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->colorBlendOp      = VK_BLEND_OP_ADD;
    pCbState->alphaSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->alphaDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->alphaBlendOp      = VK_BLEND_OP_ADD;
    pCbState->writeMask         = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    pLoState->enableLogicOp     = VK_FALSE;
    pLoState->logicOp           = VK_LOGIC_OP_NO_OP;

    pMsState->sampleMask            = SampleMask;
    pMsState->enableAlphaToCoverage = VK_FALSE;
  }

  // Explicitly instantiate here
  template class D3D11CommonContext<D3D11DeferredContext>;
  template class D3D11CommonContext<D3D11ImmediateContext>;

}
