#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_query.h"
#include "d3d11_texture.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {
  
  D3D11DeviceContext::D3D11DeviceContext(
      D3D11Device*    pParent,
      Rc<DxvkDevice>  Device)
  : m_parent  (pParent),
    m_device  (Device),
    m_csChunk (new DxvkCsChunk()) {
    // Create default state objects. We won't ever return them
    // to the application, but we'll use them to apply state.
    Com<ID3D11BlendState>         defaultBlendState;
    Com<ID3D11DepthStencilState>  defaultDepthStencilState;
    Com<ID3D11RasterizerState>    defaultRasterizerState;
    
    if (FAILED(m_parent->CreateBlendState       (nullptr, &defaultBlendState))
     || FAILED(m_parent->CreateDepthStencilState(nullptr, &defaultDepthStencilState))
     || FAILED(m_parent->CreateRasterizerState  (nullptr, &defaultRasterizerState)))
      throw DxvkError("D3D11DeviceContext: Failed to create default state objects");
    
    // Apply default state to the context. This is required
    // in order to initialize the DXVK contex properly.
    m_defaultBlendState        = static_cast<D3D11BlendState*>       (defaultBlendState.ptr());
    m_defaultDepthStencilState = static_cast<D3D11DepthStencilState*>(defaultDepthStencilState.ptr());
    m_defaultRasterizerState   = static_cast<D3D11RasterizerState*>  (defaultRasterizerState.ptr());
    
    EmitCs([
      dev     = m_device,
      bsState = m_defaultBlendState,
      dsState = m_defaultDepthStencilState,
      rsState = m_defaultRasterizerState,
      blendConst = DxvkBlendConstants {
        m_state.om.blendFactor[0], m_state.om.blendFactor[1],
        m_state.om.blendFactor[2], m_state.om.blendFactor[3] },
      stencilRef = m_state.om.stencilRef
    ] (DxvkContext* ctx) {
      ctx->beginRecording(dev->createCommandList());
      
      bsState->BindToContext(ctx, 0xFFFFFFFF);
      dsState->BindToContext(ctx);
      rsState->BindToContext(ctx);
      
      ctx->setBlendConstants  (blendConst);
      ctx->setStencilReference(stencilRef);
    });
  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::QueryInterface(
          REFIID  riid,
          void**  ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceContext);
    
    Logger::warn("D3D11DeviceContext::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_parent);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearState() {
    this->IASetInputLayout(nullptr);
    this->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    this->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    
    for (uint32_t i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
      ID3D11Buffer* buffer = nullptr;
      const UINT    offset = 0;
      const UINT    stride = 0;
      
      this->IASetVertexBuffers(i, 1, &buffer, &offset, &stride);
    }
    
    this->VSSetShader(nullptr, nullptr, 0);
    this->HSSetShader(nullptr, nullptr, 0);
    this->DSSetShader(nullptr, nullptr, 0);
    this->GSSetShader(nullptr, nullptr, 0);
    this->PSSetShader(nullptr, nullptr, 0);
    this->CSSetShader(nullptr, nullptr, 0);
    
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
      ID3D11Buffer* buffer = nullptr;
      
      this->VSSetConstantBuffers(i, 1, &buffer);
      this->HSSetConstantBuffers(i, 1, &buffer);
      this->DSSetConstantBuffers(i, 1, &buffer);
      this->GSSetConstantBuffers(i, 1, &buffer);
      this->PSSetConstantBuffers(i, 1, &buffer);
      this->CSSetConstantBuffers(i, 1, &buffer);
    }
    
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
      ID3D11ShaderResourceView* view = nullptr;
      
      this->VSSetShaderResources(i, 1, &view);
      this->HSSetShaderResources(i, 1, &view);
      this->DSSetShaderResources(i, 1, &view);
      this->GSSetShaderResources(i, 1, &view);
      this->PSSetShaderResources(i, 1, &view);
      this->CSSetShaderResources(i, 1, &view);
    }
    
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
      ID3D11SamplerState* sampler = nullptr;
      
      this->VSSetSamplers(i, 1, &sampler);
      this->HSSetSamplers(i, 1, &sampler);
      this->DSSetSamplers(i, 1, &sampler);
      this->GSSetSamplers(i, 1, &sampler);
      this->PSSetSamplers(i, 1, &sampler);
      this->CSSetSamplers(i, 1, &sampler);
    }
    
    for (uint32_t i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++) {
      ID3D11UnorderedAccessView* uav = nullptr;
      
      this->CSSetUnorderedAccessViews(i, 1, &uav, nullptr);
    }
    
    this->OMSetRenderTargets(0, nullptr, nullptr);
    this->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
    this->OMSetDepthStencilState(nullptr, 0);
    
    this->RSSetState(nullptr);
    this->RSSetViewports(0, nullptr);
    this->RSSetScissorRects(0, nullptr);
    
//     this->SOSetTargets(0, nullptr, nullptr);
    
//     this->SetPredication(nullptr, FALSE);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync) {
    Com<ID3D11Query> query;
    
    if (SUCCEEDED(pAsync->QueryInterface(__uuidof(ID3D11Query), reinterpret_cast<void**>(&query)))) {
      Com<D3D11Query> queryPtr = static_cast<D3D11Query*>(query.ptr());
      
      if (queryPtr->HasBeginEnabled()) {
        const uint32_t revision = queryPtr->Reset();
        
        EmitCs([revision, queryPtr] (DxvkContext* ctx) {
          queryPtr->Begin(ctx, revision);
        });
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::End(ID3D11Asynchronous *pAsync) {
    Com<ID3D11Query> query;
    
    if (SUCCEEDED(pAsync->QueryInterface(__uuidof(ID3D11Query), reinterpret_cast<void**>(&query)))) {
      Com<D3D11Query> queryPtr = static_cast<D3D11Query*>(query.ptr());
      
      if (queryPtr->HasBeginEnabled()) {
        EmitCs([queryPtr] (DxvkContext* ctx) {
          queryPtr->End(ctx);
        });
      } else {
        const uint32_t revision = queryPtr->Reset();
        
        EmitCs([revision, queryPtr] (DxvkContext* ctx) {
          queryPtr->Signal(ctx, revision);
        });
      }
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    // Make sure that we can safely write to the memory
    // location pointed to by pData if it is specified.
    if (DataSize == 0)
      pData = nullptr;
    
    if (pData != nullptr && pAsync->GetDataSize() != DataSize) {
      Logger::err(str::format("D3D11DeviceContext: GetData: Data size mismatch: ", pAsync->GetDataSize(), ",", DataSize));
      return E_INVALIDARG;
    }
    
    // Flush in order to make sure the query commands get dispatched
    if ((GetDataFlags & D3D11_ASYNC_GETDATA_DONOTFLUSH) == 0)
      Flush();
    
    // This method handles various different but incompatible interfaces,
    // so we have to find out what we are actually dealing with
    Com<ID3D11Query> query;
    
    if (SUCCEEDED(pAsync->QueryInterface(__uuidof(ID3D11Query), reinterpret_cast<void**>(&query))))
      return static_cast<D3D11Query*>(query.ptr())->GetData(pData, GetDataFlags);
    
    // The interface is not supported
    Logger::err("D3D11DeviceContext: GetData: Unsupported Async type");
    return E_INVALIDARG;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetPredication(
          ID3D11Predicate*                  pPredicate,
          WINBOOL                           PredicateValue) {
    Logger::err("D3D11DeviceContext::SetPredication: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GetPredication(
          ID3D11Predicate**                 ppPredicate,
          WINBOOL*                          pPredicateValue) {
    Logger::err("D3D11DeviceContext::GetPredication: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CopySubresourceRegion(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox) {
    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    
    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);
    
    if (dstResourceDim != srcResourceDim) {
      Logger::err("D3D11DeviceContext: CopySubresourceRegion: Mismatched resource types");
      return;
    }
    
    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource)->GetBufferSlice();
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource)->GetBufferSlice();
      
      VkDeviceSize srcOffset = 0;
      VkDeviceSize srcLength = srcBuffer.length();
      
      if (pSrcBox != nullptr) {
        if (pSrcBox->left > pSrcBox->right)
          return;  // no-op, but legal
        
        srcOffset = pSrcBox->left;
        srcLength = pSrcBox->right - pSrcBox->left;
      }
      
      EmitCs([
        cDstSlice = dstBuffer.subSlice(DstX, srcLength),
        cSrcSlice = srcBuffer.subSlice(srcOffset, srcLength)
      ] (DxvkContext* ctx) {
        ctx->copyBuffer(
          cDstSlice.buffer(),
          cDstSlice.offset(),
          cSrcSlice.buffer(),
          cSrcSlice.offset(),
          cSrcSlice.length());
      });
    } else {
      const D3D11TextureInfo* dstTextureInfo = GetCommonTextureInfo(pDstResource);
      const D3D11TextureInfo* srcTextureInfo = GetCommonTextureInfo(pSrcResource);
      
      const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstTextureInfo->image->info().format);
      const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcTextureInfo->image->info().format);
      
      const VkImageSubresource dstSubresource = GetSubresourceFromIndex(dstFormatInfo->aspectMask, dstTextureInfo->image->info().mipLevels, DstSubresource);
      const VkImageSubresource srcSubresource = GetSubresourceFromIndex(srcFormatInfo->aspectMask, srcTextureInfo->image->info().mipLevels, SrcSubresource);
      
      VkOffset3D srcOffset = { 0, 0, 0 };
      VkOffset3D dstOffset = {
        static_cast<int32_t>(DstX),
        static_cast<int32_t>(DstY),
        static_cast<int32_t>(DstZ) };
      
      VkExtent3D extent = srcTextureInfo->image->mipLevelExtent(srcSubresource.mipLevel);
      
      if (pSrcBox != nullptr) {
        if (pSrcBox->left  >= pSrcBox->right
         || pSrcBox->top   >= pSrcBox->bottom
         || pSrcBox->front >= pSrcBox->back)
          return;  // no-op, but legal
        
        srcOffset.x = pSrcBox->left;
        srcOffset.y = pSrcBox->top;
        srcOffset.z = pSrcBox->front;
        
        extent.width  = pSrcBox->right -  pSrcBox->left;
        extent.height = pSrcBox->bottom - pSrcBox->top;
        extent.depth  = pSrcBox->back -   pSrcBox->front;
      }
      
      const VkImageSubresourceLayers dstLayers = {
        dstSubresource.aspectMask,
        dstSubresource.mipLevel,
        dstSubresource.arrayLayer, 1 };
      
      const VkImageSubresourceLayers srcLayers = {
        srcSubresource.aspectMask,
        srcSubresource.mipLevel,
        srcSubresource.arrayLayer, 1 };
      
      EmitCs([
        cDstImage  = dstTextureInfo->image,
        cSrcImage  = srcTextureInfo->image,
        cDstLayers = dstLayers,
        cSrcLayers = srcLayers,
        cDstOffset = dstOffset,
        cSrcOffset = srcOffset,
        cExtent    = extent
      ] (DxvkContext* ctx) {
        ctx->copyImage(
          cDstImage, cDstLayers, cDstOffset,
          cSrcImage, cSrcLayers, cSrcOffset,
          cExtent);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    
    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);
    
    if (dstResourceDim != srcResourceDim) {
      Logger::err("D3D11DeviceContext: CopyResource: Mismatched resource types");
      return;
    }
    
    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource)->GetBufferSlice();
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource)->GetBufferSlice();
      
      if (dstBuffer.length() != srcBuffer.length()) {
        Logger::err("D3D11DeviceContext: CopyResource: Mismatched buffer size");
        return;
      }
      
      EmitCs([
        cDstBuffer = std::move(dstBuffer),
        cSrcBuffer = std::move(srcBuffer)
      ] (DxvkContext* ctx) {
        ctx->copyBuffer(
          cDstBuffer.buffer(),
          cDstBuffer.offset(),
          cSrcBuffer.buffer(),
          cSrcBuffer.offset(),
          cSrcBuffer.length());
      });
    } else {
      const D3D11TextureInfo* dstTextureInfo = GetCommonTextureInfo(pDstResource);
      const D3D11TextureInfo* srcTextureInfo = GetCommonTextureInfo(pSrcResource);

      const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstTextureInfo->image->info().format);
      const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcTextureInfo->image->info().format);

      for (uint32_t i = 0; i < srcTextureInfo->image->info().mipLevels; i++) {
        VkExtent3D extent = srcTextureInfo->image->mipLevelExtent(i);

        const VkImageSubresourceLayers dstLayers = { dstFormatInfo->aspectMask, i, 0, dstTextureInfo->image->info().numLayers };
        const VkImageSubresourceLayers srcLayers = { srcFormatInfo->aspectMask, i, 0, srcTextureInfo->image->info().numLayers };
        
        EmitCs([
          cDstImage  = dstTextureInfo->image,
          cSrcImage  = srcTextureInfo->image,
          cDstLayers = dstLayers,
          cSrcLayers = srcLayers,
          cExtent    = extent
        ] (DxvkContext* ctx) {
          ctx->copyImage(
            cDstImage, cDstLayers, VkOffset3D { 0, 0, 0 },
            cSrcImage, cSrcLayers, VkOffset3D { 0, 0, 0 },
            cExtent);
        });
      }
    }
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::CopyStructureCount(
          ID3D11Buffer*                     pDstBuffer,
          UINT                              DstAlignedByteOffset,
          ID3D11UnorderedAccessView*        pSrcView) {
    auto buf = static_cast<D3D11Buffer*>(pDstBuffer);
    auto uav = static_cast<D3D11UnorderedAccessView*>(pSrcView);

    EmitCs([
      cDstSlice = buf->GetBufferSlice(DstAlignedByteOffset),
      cSrcSlice = uav->GetCounterSlice()
    ] (DxvkContext* ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset(),
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        sizeof(uint32_t));
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearRenderTargetView(
          ID3D11RenderTargetView*           pRenderTargetView,
    const FLOAT                             ColorRGBA[4]) {
    auto rtv = static_cast<D3D11RenderTargetView*>(pRenderTargetView);
    
    if (rtv == nullptr)
      return;
    
    // Find out whether the given attachment is currently bound
    // or not, and if it is, which attachment index it has.
    int32_t attachmentIndex = -1;
    
    for (uint32_t i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      if (m_state.om.renderTargetViews.at(i) == rtv)
        attachmentIndex = i;
    }
    
    // Copy the clear color into a clear value structure.
    // This should also work for images that don nott have
    // a floating point format.
    const Rc<DxvkImageView> view = rtv->GetImageView();
    
    VkClearColorValue clearValue;
    std::memcpy(clearValue.float32, ColorRGBA,
      sizeof(clearValue.float32));
    
    if (attachmentIndex >= 0) {
      // Image is bound to the pipeline for rendering. We can
      // use the clear function that operates on attachments.
      VkClearAttachment clearInfo;
      clearInfo.aspectMask          = VK_IMAGE_ASPECT_COLOR_BIT;
      clearInfo.colorAttachment     = static_cast<uint32_t>(attachmentIndex);
      clearInfo.clearValue.color    = clearValue;
      
      // Clear the full area. On FL 9.x, only the first array
      // layer will be cleared, rather than all array layers.
      VkClearRect clearRect;
      clearRect.rect.offset.x       = 0;
      clearRect.rect.offset.y       = 0;
      clearRect.rect.extent.width   = view->mipLevelExtent(0).width;
      clearRect.rect.extent.height  = view->mipLevelExtent(0).height;
      clearRect.baseArrayLayer      = 0;
      clearRect.layerCount          = view->info().numLayers;
      
      if (m_parent->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        clearRect.layerCount        = 1;
      
      EmitCs([
        cClearInfo = clearInfo,
        cClearRect = clearRect
      ] (DxvkContext* ctx) {
        ctx->clearRenderTarget(cClearInfo, cClearRect);
      });
    } else {
      // Image is not bound to the pipeline. We can still clear
      // it, but we'll have to use a generic clear function.
      EmitCs([
        cClearValue = clearValue,
        cDstView    = view
      ] (DxvkContext* ctx) {
        ctx->clearColorImage(cDstView->image(),
          cClearValue, cDstView->subresources());
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewUint(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const UINT                              Values[4]) {
    auto uav = static_cast<D3D11UnorderedAccessView*>(pUnorderedAccessView);
    
    if (uav == nullptr)
      return;
    
    if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
      Logger::err("D3D11: ClearUnorderedAccessViewUint: Not supported for buffers");
    } else {
      VkClearColorValue clearValue;
      
      for (uint32_t i = 0; i < 4; i++)
        clearValue.uint32[i] = Values[i];
      
      EmitCs([
        cClearValue = clearValue,
        cDstView    = uav->GetImageView()
      ] (DxvkContext* ctx) {
        ctx->clearColorImage(cDstView->image(),
          cClearValue, cDstView->subresources());
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewFloat(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const FLOAT                             Values[4]) {
    Logger::err("D3D11DeviceContext::ClearUnorderedAccessViewFloat: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearDepthStencilView(
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
    
    if (dsv == nullptr)
      return;
    
    // Figure out which aspects to clear based
    // on the image format and the clear flags.
    const Rc<DxvkImageView> view = dsv->GetImageView();
    
    VkImageAspectFlags aspectMask = 0;
    
    if (ClearFlags & D3D11_CLEAR_DEPTH)
      aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    
    if (ClearFlags & D3D11_CLEAR_STENCIL)
      aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    
    const DxvkFormatInfo* formatInfo =
      imageFormatInfo(view->info().format);
    aspectMask &= formatInfo->aspectMask;
    
    VkClearDepthStencilValue clearValue;
    clearValue.depth   = Depth;
    clearValue.stencil = Stencil;
    
    if (m_state.om.depthStencilView == dsv) {
      // Image is bound to the pipeline for rendering. We can
      // use the clear function that operates on attachments.
      VkClearAttachment clearInfo;
      clearInfo.aspectMask              = aspectMask;
      clearInfo.colorAttachment         = 0;
      clearInfo.clearValue.depthStencil = clearValue;
      
      // Clear the full area
      VkClearRect clearRect;
      clearRect.rect.offset.x       = 0;
      clearRect.rect.offset.y       = 0;
      clearRect.rect.extent.width   = view->mipLevelExtent(0).width;
      clearRect.rect.extent.height  = view->mipLevelExtent(0).height;
      clearRect.baseArrayLayer      = 0;
      clearRect.layerCount          = view->info().numLayers;
      
      // FIXME Is this correct? Docs don't say anything
      if (m_parent->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        clearRect.layerCount        = 1;
      
      EmitCs([
        cClearInfo = clearInfo,
        cClearRect = clearRect
      ] (DxvkContext* ctx) {
        ctx->clearRenderTarget(cClearInfo, cClearRect);
      });
    } else {
      EmitCs([
        cClearValue = clearValue,
        cDstView    = view,
        cAspectMask = aspectMask
      ] (DxvkContext* ctx) {
        VkImageSubresourceRange subresources = cDstView->subresources();
        subresources.aspectMask = cAspectMask;
        
        ctx->clearDepthStencilImage(cDstView->image(),
          cClearValue, subresources);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) {
    auto view = static_cast<D3D11ShaderResourceView*>(pShaderResourceView);
      
    if (view->GetResourceType() != D3D11_RESOURCE_DIMENSION_BUFFER) {
      EmitCs([cDstImageView = view->GetImageView()]
      (DxvkContext* ctx) {
        ctx->generateMipmaps(
          cDstImageView->image(),
          cDstImageView->subresources());
      });
    } else {
      Logger::err("D3D11DeviceContext: GenerateMips called on a buffer");
    }
    
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    // We need a different code path for buffers
    D3D11_RESOURCE_DIMENSION resourceType;
    pDstResource->GetType(&resourceType);
    
    if (resourceType == D3D11_RESOURCE_DIMENSION_BUFFER) {
      const auto bufferResource = static_cast<D3D11Buffer*>(pDstResource);
      const auto bufferSlice = bufferResource->GetBufferSlice();
      
      VkDeviceSize offset = bufferSlice.offset();
      VkDeviceSize size   = bufferSlice.length();
      
      if (pDstBox != nullptr) {
        offset = pDstBox->left;
        size   = pDstBox->right - pDstBox->left;
      }
      
      if (offset + size > bufferSlice.length()) {
        Logger::err("D3D11DeviceContext: Buffer update range out of bounds");
        return;
      }
      
      if (size == 0)
        return;
      
      if (((size == bufferSlice.length())
       && (bufferSlice.buffer()->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
        D3D11_MAPPED_SUBRESOURCE mappedSr;
        Map(pDstResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSr);
        std::memcpy(mappedSr.pData, pSrcData, size);
        Unmap(pDstResource, 0);
      } else {
        DxvkDataSlice dataSlice = AllocUpdateBufferSlice(size);
        std::memcpy(dataSlice.ptr(), pSrcData, size);
        
        EmitCs([
          cDataBuffer   = std::move(dataSlice),
          cBufferSlice  = bufferSlice.subSlice(offset, size)
        ] (DxvkContext* ctx) {
          ctx->updateBuffer(
            cBufferSlice.buffer(),
            cBufferSlice.offset(),
            cBufferSlice.length(),
            cDataBuffer.ptr());
        });
      }
    } else {
      const D3D11TextureInfo* textureInfo
        = GetCommonTextureInfo(pDstResource);
      
      const VkImageSubresource subresource =
        GetSubresourceFromIndex(VK_IMAGE_ASPECT_COLOR_BIT,
          textureInfo->image->info().mipLevels, DstSubresource);
      
      VkOffset3D offset = { 0, 0, 0 };
      VkExtent3D extent = textureInfo->image->mipLevelExtent(subresource.mipLevel);
      
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
      
      const VkImageSubresourceLayers layers = {
        subresource.aspectMask,
        subresource.mipLevel,
        subresource.arrayLayer, 1 };
      
      auto formatInfo = imageFormatInfo(
        textureInfo->image->info().format);
      
      const VkExtent3D regionExtent = util::computeBlockCount(extent, formatInfo->blockSize);
      
      const VkDeviceSize bytesPerRow   = regionExtent.width  * formatInfo->elementSize;
      const VkDeviceSize bytesPerLayer = regionExtent.height * bytesPerRow;
      const VkDeviceSize bytesTotal    = regionExtent.depth  * bytesPerLayer;
      
      DxvkDataSlice imageDataBuffer = AllocUpdateBufferSlice(bytesTotal);
      
      util::packImageData(
        reinterpret_cast<char*>(imageDataBuffer.ptr()),
        reinterpret_cast<const char*>(pSrcData),
        regionExtent, formatInfo->elementSize,
        SrcRowPitch, SrcDepthPitch);
      
      EmitCs([
        cDstImage         = textureInfo->image,
        cDstLayers        = layers,
        cDstOffset        = offset,
        cDstExtent        = extent,
        cSrcData          = std::move(imageDataBuffer),
        cSrcBytesPerRow   = bytesPerRow,
        cSrcBytesPerLayer = bytesPerLayer
      ] (DxvkContext* ctx) {
        ctx->updateImage(cDstImage, cDstLayers,
          cDstOffset, cDstExtent, cSrcData.ptr(),
          cSrcBytesPerRow, cSrcBytesPerLayer);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetResourceMinLOD(
          ID3D11Resource*                   pResource,
          FLOAT                             MinLOD) {
    Logger::err("D3D11DeviceContext::SetResourceMinLOD: Not implemented");
  }
  
  
  FLOAT STDMETHODCALLTYPE D3D11DeviceContext::GetResourceMinLOD(ID3D11Resource* pResource) {
    Logger::err("D3D11DeviceContext::GetResourceMinLOD: Not implemented");
    return 0.0f;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ResolveSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    D3D11_RESOURCE_DIMENSION dstResourceType;
    D3D11_RESOURCE_DIMENSION srcResourceType;
    
    pDstResource->GetType(&dstResourceType);
    pSrcResource->GetType(&srcResourceType);
    
    if (dstResourceType != D3D11_RESOURCE_DIMENSION_TEXTURE2D
     || srcResourceType != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::err("D3D11: ResolveSubresource: Incompatible resources");
      return;
    }
    
    auto dstTexture = static_cast<D3D11Texture2D*>(pDstResource);
    auto srcTexture = static_cast<D3D11Texture2D*>(pSrcResource);
    
    D3D11_TEXTURE2D_DESC dstDesc;
    D3D11_TEXTURE2D_DESC srcDesc;
    
    dstTexture->GetDesc(&dstDesc);
    srcTexture->GetDesc(&srcDesc);
    
    if (dstDesc.SampleDesc.Count != 1) {
      Logger::err("D3D11: ResolveSubresource: Resource sample count invalid");
      return;
    }
    
    const D3D11TextureInfo* dstTextureInfo = GetCommonTextureInfo(pDstResource);
    const D3D11TextureInfo* srcTextureInfo = GetCommonTextureInfo(pSrcResource);
    
    const DxgiFormatInfo dstFormatInfo = m_parent->LookupFormat(dstDesc.Format, DxgiFormatMode::Any);
    const DxgiFormatInfo srcFormatInfo = m_parent->LookupFormat(srcDesc.Format, DxgiFormatMode::Any);
    
    const VkImageSubresource dstSubresource =
      GetSubresourceFromIndex(dstFormatInfo.aspect,
        dstTextureInfo->image->info().mipLevels, DstSubresource);
    
    const VkImageSubresource srcSubresource =
      GetSubresourceFromIndex(srcFormatInfo.aspect,
        srcTextureInfo->image->info().mipLevels, SrcSubresource);
    
    const VkImageSubresourceLayers dstSubresourceLayers = {
      dstSubresource.aspectMask,
      dstSubresource.mipLevel,
      dstSubresource.arrayLayer, 1 };
    
    const VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };
    
    if (srcDesc.SampleDesc.Count == 1) {
      EmitCs([
        cDstImage  = dstTextureInfo->image,
        cSrcImage  = srcTextureInfo->image,
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
        Format, DxgiFormatMode::Any).format;
      
      EmitCs([
        cDstImage  = dstTextureInfo->image,
        cSrcImage  = srcTextureInfo->image,
        cDstSubres = dstSubresourceLayers,
        cSrcSubres = srcSubresourceLayers,
        cFormat    = format
      ] (DxvkContext* ctx) {
        ctx->resolveImage(
          cDstImage, cDstSubres,
          cSrcImage, cSrcSubres,
          cFormat);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawAuto() {
    Logger::err("D3D11DeviceContext::DrawAuto: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Draw(
          UINT            VertexCount,
          UINT            StartVertexLocation) {
    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCount, 1,
        StartVertexLocation, 0);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexed(
          UINT            IndexCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation) {
    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCount, 1,
        StartIndexLocation,
        BaseVertexLocation, 0);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstanced(
          UINT            VertexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartVertexLocation,
          UINT            StartInstanceLocation) {
    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCountPerInstance,
        InstanceCount,
        StartVertexLocation,
        StartInstanceLocation);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstanced(
          UINT            IndexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation,
          UINT            StartInstanceLocation) {
    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCountPerInstance,
        InstanceCount,
        StartIndexLocation,
        BaseVertexLocation,
        StartInstanceLocation);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pBufferForArgs);
    
    EmitCs([bufferSlice = buffer->GetBufferSlice(AlignedByteOffsetForArgs)]
    (DxvkContext* ctx) {
      ctx->drawIndexedIndirect(
        bufferSlice, 1, 0);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pBufferForArgs);
    
    EmitCs([bufferSlice = buffer->GetBufferSlice(AlignedByteOffsetForArgs)]
    (DxvkContext* ctx) {
      ctx->drawIndirect(bufferSlice, 1, 0);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Dispatch(
          UINT            ThreadGroupCountX,
          UINT            ThreadGroupCountY,
          UINT            ThreadGroupCountZ) {
    EmitCs([=] (DxvkContext* ctx) {
      ctx->dispatch(
        ThreadGroupCountX,
        ThreadGroupCountY,
        ThreadGroupCountZ);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DispatchIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pBufferForArgs);
    
    EmitCs([bufferSlice = buffer->GetBufferSlice(AlignedByteOffsetForArgs)]
    (DxvkContext* ctx) {
      ctx->dispatchIndirect(bufferSlice);
    });
    
    m_drawCount += 1;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
    Com<D3D11InputLayout> inputLayout =
      static_cast<D3D11InputLayout*>(pInputLayout);
    
    if (m_state.ia.inputLayout != inputLayout) {
      m_state.ia.inputLayout = inputLayout;
      
      if (inputLayout != nullptr) {
        EmitCs([inputLayout] (DxvkContext* ctx) {
          inputLayout->BindToContext(ctx);
        });
      } else {
        EmitCs([inputLayout] (DxvkContext* ctx) {
          ctx->setInputLayout(0, nullptr, 0, nullptr);
        });
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    if (m_state.ia.primitiveTopology != Topology) {
      m_state.ia.primitiveTopology = Topology;
      
      if (Topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
        return;
      
      const DxvkInputAssemblyState iaState = [Topology] () -> DxvkInputAssemblyState {
        if (Topology >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
         && Topology <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST) {
          // Tessellation patch. The number of control points per
          // patch can be inferred from the enum value in D3D11.
          return { VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE,
            uint32_t(Topology - D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1) };
        } else {
          switch (Topology) {
            case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
              return { VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
              return { VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
              return { VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_TRUE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
              return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE, 0 };
              
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
              return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
              return { VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_FALSE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
              return { VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_TRUE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
              return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_FALSE, 0 };
            
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
              return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_TRUE, 0 };
            
            default:
              Logger::err(str::format("D3D11: Invalid primitive topology: ", Topology));
              return { };
          }
        }
      }();
      
      EmitCs([iaState] (DxvkContext* ctx) {
        ctx->setInputAssemblyState(iaState);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    // TODO check if any of these buffers
    // are bound as UAVs or stream outputs
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppVertexBuffers[i]);
      
      m_state.ia.vertexBuffers[i].buffer = newBuffer;
      m_state.ia.vertexBuffers[i].offset = pOffsets[i];
      m_state.ia.vertexBuffers[i].stride = pStrides[i];
      
      if (newBuffer != nullptr) {
        EmitCs([
          slotId = StartSlot + i,
          offset = pOffsets[i],
          stride = pStrides[i],
          slice  = newBuffer->GetBufferSlice(pOffsets[i])
        ] (DxvkContext* ctx) {
          ctx->bindVertexBuffer(
            slotId, slice, stride);
        });
      } else {
        EmitCs([cSlotId = StartSlot + i] (DxvkContext* ctx) {
          ctx->bindVertexBuffer(cSlotId, DxvkBufferSlice(), 0);
        });
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    auto newBuffer = static_cast<D3D11Buffer*>(pIndexBuffer);
    
    m_state.ia.indexBuffer.buffer = newBuffer;
    m_state.ia.indexBuffer.offset = Offset;
    m_state.ia.indexBuffer.format = Format;
    
    // As in Vulkan, the index format can be either a 32-bit
    // unsigned integer or a 16-bit unsigned integer, no other
    // formats are allowed.
    if (newBuffer != nullptr) {
      VkIndexType indexType = VK_INDEX_TYPE_UINT32;
      
      switch (Format) {
        case DXGI_FORMAT_R16_UINT: indexType = VK_INDEX_TYPE_UINT16; break;
        case DXGI_FORMAT_R32_UINT: indexType = VK_INDEX_TYPE_UINT32; break;
        default: Logger::err(str::format("D3D11: Invalid index format: ", Format));
      }
      
      EmitCs([indexType,
        slice = newBuffer->GetBufferSlice(Offset)
      ] (DxvkContext* ctx) {
        ctx->bindIndexBuffer(slice, indexType);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) {
    *ppInputLayout = m_state.ia.inputLayout.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) {
    *pTopology = m_state.ia.primitiveTopology;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppVertexBuffers,
          UINT*                             pStrides,
          UINT*                             pOffsets) {
    for (uint32_t i = 0; i < NumBuffers; i++) {
      if (ppVertexBuffers != nullptr)
        ppVertexBuffers[i] = m_state.ia.vertexBuffers[StartSlot + i].buffer.ref();
      
      if (pStrides != nullptr)
        pStrides[i] = m_state.ia.vertexBuffers[StartSlot + i].stride;
      
      if (pOffsets != nullptr)
        pOffsets[i] = m_state.ia.vertexBuffers[StartSlot + i].offset;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetIndexBuffer(
          ID3D11Buffer**                    ppIndexBuffer,
          DXGI_FORMAT*                      pFormat,
          UINT*                             pOffset) {
    if (ppIndexBuffer != nullptr)
      *ppIndexBuffer = m_state.ia.indexBuffer.buffer.ref();
    
    if (pFormat != nullptr)
      *pFormat = m_state.ia.indexBuffer.format;
    
    if (pOffset != nullptr)
      *pOffset = m_state.ia.indexBuffer.offset;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetShader(
          ID3D11VertexShader*               pVertexShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11VertexShader*>(pVertexShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::VSSetShader: Class instances not supported");
    
    if (m_state.vs.shader != shader) {
      m_state.vs.shader = shader;
      
      EmitCs([cShader = shader != nullptr ? shader->GetShader() : nullptr]
      (DxvkContext* ctx) {
        ctx->bindShader(VK_SHADER_STAGE_VERTEX_BIT, cShader);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::VertexShader,
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::VertexShader,
      m_state.vs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::VertexShader,
      m_state.vs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShader(
          ID3D11VertexShader**              ppVertexShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppVertexShader != nullptr)
      *ppVertexShader = m_state.vs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.vs.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.vs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.vs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShader(
          ID3D11HullShader*                 pHullShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    if (m_state.hs.shader.ptr() != pHullShader)
      Logger::err("D3D11DeviceContext::HSSetShader: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::HullShader,
      m_state.hs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::HullShader,
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::HullShader,
      m_state.hs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShader(
          ID3D11HullShader**                ppHullShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppHullShader != nullptr)
      *ppHullShader = m_state.hs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.hs.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.hs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.hs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShader(
          ID3D11DomainShader*               pDomainShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    if (m_state.ds.shader.ptr() != pDomainShader)
      Logger::err("D3D11DeviceContext::DSSetShader: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::DomainShader,
      m_state.ds.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::DomainShader,
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::DomainShader,
      m_state.ds.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShader(
          ID3D11DomainShader**              ppDomainShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppDomainShader != nullptr)
      *ppDomainShader = m_state.ds.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.ds.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.ds.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.ds.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShader(
          ID3D11GeometryShader*             pShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11GeometryShader*>(pShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::GSSetShader: Class instances not supported");
    
    if (m_state.gs.shader != shader) {
      m_state.gs.shader = shader;
      
      EmitCs([cShader = shader != nullptr ? shader->GetShader() : nullptr]
      (DxvkContext* ctx) {
        ctx->bindShader(VK_SHADER_STAGE_GEOMETRY_BIT, cShader);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::GeometryShader,
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::GeometryShader,
      m_state.gs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::GeometryShader,
      m_state.gs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShader(
          ID3D11GeometryShader**            ppGeometryShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppGeometryShader != nullptr)
      *ppGeometryShader = m_state.gs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.gs.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.gs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.gs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShader(
          ID3D11PixelShader*                pPixelShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11PixelShader*>(pPixelShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::PSSetShader: Class instances not supported");
    
    if (m_state.ps.shader != shader) {
      m_state.ps.shader = shader;
      
      EmitCs([cShader = shader != nullptr ? shader->GetShader() : nullptr]
      (DxvkContext* ctx) {
        ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, cShader);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::PixelShader,
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::PixelShader,
      m_state.ps.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::PixelShader,
      m_state.ps.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShader(
          ID3D11PixelShader**               ppPixelShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppPixelShader != nullptr)
      *ppPixelShader = m_state.ps.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.ps.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.ps.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.ps.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShader(
          ID3D11ComputeShader*              pComputeShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11ComputeShader*>(pComputeShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::CSSetShader: Class instances not supported");
    
    if (m_state.cs.shader != shader) {
      m_state.cs.shader = shader;
      
      EmitCs([cShader = shader != nullptr ? shader->GetShader() : nullptr]
      (DxvkContext* ctx) {
        ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, cShader);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::ComputeShader,
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::ComputeShader,
      m_state.cs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::ComputeShader,
      m_state.cs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    this->BindUnorderedAccessViews(
      DxbcProgramType::ComputeShader,
      m_state.cs.unorderedAccessViews,
      StartSlot, NumUAVs,
      ppUnorderedAccessViews);
    
    if (pUAVInitialCounts != nullptr) {
      this->InitUnorderedAccessViewCounters(
        NumUAVs, ppUnorderedAccessViews,
        pUAVInitialCounts);
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShader(
          ID3D11ComputeShader**             ppComputeShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    if (ppComputeShader != nullptr)
      *ppComputeShader = m_state.cs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppConstantBuffers[i] = m_state.cs.constantBuffers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.cs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.cs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    Logger::err("D3D11DeviceContext::CSGetUnorderedAccessViews: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    // Optimization: If the number of draw and dispatch calls issued
    // prior to the previous context flush is above a certain threshold,
    // submit the current command buffer in order to keep the GPU busy.
    // This also helps keep the command buffers at a reasonable size.
    if (m_drawCount >= 500)
      Flush();
    
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      D3D11RenderTargetView* view = nullptr;
      
      if ((i < NumViews) && (ppRenderTargetViews[i] != nullptr))
        view = static_cast<D3D11RenderTargetView*>(ppRenderTargetViews[i]);
      
      m_state.om.renderTargetViews.at(i) = view;
    }
    
    m_state.om.depthStencilView = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
    
    // NOTE According to the Microsoft docs, we are supposed to
    // unbind overlapping shader resource views. Since this comes
    // with a large performance penalty we'll ignore this until an
    // application actually relies on this behaviour.
    DxvkRenderTargets attachments;
    
    // D3D11 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    if (ppRenderTargetViews != nullptr || pDepthStencilView != nullptr) {
      for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
        if (m_state.om.renderTargetViews.at(i) != nullptr) {
          attachments.setColorTarget(i,
            m_state.om.renderTargetViews.at(i)->GetImageView(),
            m_state.om.renderTargetViews.at(i)->GetRenderLayout());
        }
      }
      
      if (m_state.om.depthStencilView != nullptr) {
        attachments.setDepthTarget(
          m_state.om.depthStencilView->GetImageView(),
          m_state.om.depthStencilView->GetRenderLayout());
      }
    }
    
    // Create and bind the framebuffer object to the context
    EmitCs([attachments, dev = m_device] (DxvkContext* ctx) {
      Rc<DxvkFramebuffer> framebuffer = nullptr;
      if (attachments.hasAttachments())
        framebuffer = dev->createFramebuffer(attachments);
      ctx->bindFramebuffer(framebuffer);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
      OMSetRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView);
    
    if (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS) {
      // UAVs are made available to all shader stages in
      // the graphics pipeline even though this code may
      // suggest that they are limited to the pixel shader.
      // This behaviour is only required for FL_11_1.
      BindUnorderedAccessViews(
        DxbcProgramType::PixelShader,
        m_state.ps.unorderedAccessViews,
        UAVStartSlot, NumUAVs,
        ppUnorderedAccessViews);
      
      if (pUAVInitialCounts != nullptr) {
        InitUnorderedAccessViewCounters(NumUAVs,
          ppUnorderedAccessViews, pUAVInitialCounts);
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetBlendState(
          ID3D11BlendState*                 pBlendState,
    const FLOAT                             BlendFactor[4],
          UINT                              SampleMask) {
    Com<D3D11BlendState> blendState =
      static_cast<D3D11BlendState*>(pBlendState);
    
    if (m_state.om.cbState    != blendState
     || m_state.om.sampleMask != SampleMask) {
      m_state.om.cbState    = blendState;
      m_state.om.sampleMask = SampleMask;
      
      if (blendState == nullptr)
        blendState = m_defaultBlendState;
      
      EmitCs([
        cBlendState = std::move(blendState),
        cSampleMask = SampleMask
      ] (DxvkContext* ctx) {
        cBlendState->BindToContext(ctx, cSampleMask);
      });
    }
    
    if (BlendFactor != nullptr) {
      bool updateBlendFactor = false;
      
      for (uint32_t i = 0; i < 4; i++) {
        updateBlendFactor |= m_state.om.blendFactor[i] != BlendFactor[i];
        m_state.om.blendFactor[i] = BlendFactor[i];
      }
      
      if (updateBlendFactor) {
        EmitCs([
          cBlendConstants = DxvkBlendConstants {
            BlendFactor[0], BlendFactor[1],
            BlendFactor[2], BlendFactor[3] }
        ] (DxvkContext* ctx) {
          ctx->setBlendConstants(cBlendConstants);
        });
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetDepthStencilState(
          ID3D11DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    Com<D3D11DepthStencilState> depthStencilState =
      static_cast<D3D11DepthStencilState*>(pDepthStencilState);
    
    if (m_state.om.dsState != depthStencilState) {
      m_state.om.dsState = depthStencilState;
      
      if (depthStencilState == nullptr)
        depthStencilState = m_defaultDepthStencilState;
      
      EmitCs([cDepthStencilState = std::move(depthStencilState)]
      (DxvkContext* ctx) {
        cDepthStencilState->BindToContext(ctx);
      });
    }
    
    if (m_state.om.stencilRef != StencilRef) {
      m_state.om.stencilRef = StencilRef;
      
      EmitCs([cStencilRef = StencilRef] (DxvkContext* ctx) {
        ctx->setStencilReference(cStencilRef);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    if (ppRenderTargetViews != nullptr) {
      for (UINT i = 0; i < NumViews; i++)
        ppRenderTargetViews[i] = i < m_state.om.renderTargetViews.size()
          ? m_state.om.renderTargetViews.at(i).ref()
          : nullptr;
    }
    
    if (ppDepthStencilView != nullptr)
      *ppDepthStencilView = m_state.om.depthStencilView.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    Logger::err("D3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetBlendState(
          ID3D11BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    if (ppBlendState != nullptr)
      *ppBlendState = m_state.om.cbState.ref();
    
    if (BlendFactor != nullptr)
      std::memcpy(BlendFactor, m_state.om.blendFactor, sizeof(FLOAT) * 4);
    
    if (pSampleMask != nullptr)
      *pSampleMask = m_state.om.sampleMask;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetDepthStencilState(
          ID3D11DepthStencilState**         ppDepthStencilState,
          UINT*                             pStencilRef) {
    if (ppDepthStencilState != nullptr)
      *ppDepthStencilState = m_state.om.dsState.ref();
    
    if (pStencilRef != nullptr)
      *pStencilRef = m_state.om.stencilRef;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    Com<D3D11RasterizerState> rasterizerState =
      static_cast<D3D11RasterizerState*>(pRasterizerState);
    
    if (m_state.rs.state != rasterizerState) {
      m_state.rs.state = rasterizerState;
      
      if (rasterizerState == nullptr)
        rasterizerState = m_defaultRasterizerState;
      
      EmitCs([cRasterizerState = std::move(rasterizerState)]
      (DxvkContext* ctx) {
        cRasterizerState->BindToContext(ctx);
      });
      
      // In D3D11, the rasterizer state defines
      // whether the scissor test is enabled, so
      // we have to update the scissor rectangles.
      this->ApplyViewportState();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetViewports(
          UINT                              NumViewports,
    const D3D11_VIEWPORT*                   pViewports) {
    m_state.rs.numViewports = NumViewports;
    
    for (uint32_t i = 0; i < NumViewports; i++)
      m_state.rs.viewports.at(i) = pViewports[i];
    
    this->ApplyViewportState();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetScissorRects(
          UINT                              NumRects,
    const D3D11_RECT*                       pRects) {
    m_state.rs.numScissors = NumRects;
    
    for (uint32_t i = 0; i < NumRects; i++)
      m_state.rs.scissors.at(i) = pRects[i];
    
    if (m_state.rs.state != nullptr) {
      D3D11_RASTERIZER_DESC rsDesc;
      m_state.rs.state->GetDesc(&rsDesc);
      
      if (rsDesc.ScissorEnable)
        this->ApplyViewportState();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    if (ppRasterizerState != nullptr)
      *ppRasterizerState = m_state.rs.state.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    if (pViewports != nullptr) {
      for (uint32_t i = 0; i < *pNumViewports; i++) {
        if (i < m_state.rs.numViewports) {
          pViewports[i] = m_state.rs.viewports.at(i);
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
    
    *pNumViewports = m_state.rs.numViewports;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetScissorRects(
          UINT*                             pNumRects,
          D3D11_RECT*                       pRects) {
    if (pRects != nullptr) {
      for (uint32_t i = 0; i < *pNumRects; i++) {
        if (i < m_state.rs.numScissors) {
          pRects[i] = m_state.rs.scissors.at(i);
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SOSetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppSOTargets,
    const UINT*                             pOffsets) {
    if (NumBuffers > 0)
      Logger::err("D3D11DeviceContext::SOSetTargets: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SOGetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets) {
    if (NumBuffers > 0)
      Logger::err("D3D11DeviceContext::SOGetTargets: Not implemented");
  }
  
  
  void D3D11DeviceContext::BindConstantBuffers(
          DxbcProgramType                   ShaderStage,
          D3D11ConstantBufferBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::ConstantBuffer,
      StartSlot);
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);
      
      if (Bindings[StartSlot + i] != newBuffer) {
        Bindings[StartSlot + i] = newBuffer;
        
        EmitCs([
          cSlotId = slotId + i,
          cSlice  = newBuffer != nullptr
            ? newBuffer->GetBufferSlice()
            : DxvkBufferSlice()
        ] (DxvkContext* ctx) {
          ctx->bindResourceBuffer(cSlotId, cSlice);
        });
      }
    }
  }
  
  
  void D3D11DeviceContext::BindSamplers(
          DxbcProgramType                   ShaderStage,
          D3D11SamplerBindings&             Bindings,
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::ImageSampler,
      StartSlot);
    
    for (uint32_t i = 0; i < NumSamplers; i++) {
      auto sampler = static_cast<D3D11SamplerState*>(ppSamplers[i]);
      
      if (Bindings[StartSlot + i] != sampler) {
        Bindings[StartSlot + i] = sampler;
        
        EmitCs([
          cSlotId  = slotId + i,
          cSampler = sampler != nullptr
            ? sampler->GetDXVKSampler()
            : nullptr
        ] (DxvkContext* ctx) {
          ctx->bindResourceSampler(cSlotId, cSampler);
        });
      }
    }
  }
  
  
  void D3D11DeviceContext::BindShaderResources(
          DxbcProgramType                   ShaderStage,
          D3D11ShaderResourceBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumResources,
          ID3D11ShaderResourceView* const*  ppResources) {
    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::ShaderResource,
      StartSlot);
    
    for (uint32_t i = 0; i < NumResources; i++) {
      auto resView = static_cast<D3D11ShaderResourceView*>(ppResources[i]);
      
      if (Bindings[StartSlot + i] != resView) {
        Bindings[StartSlot + i] = resView;
        
        if (resView != nullptr) {
          // Figure out what we have to bind based on the resource type
          if (resView->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
            EmitCs([cSlotId = slotId + i, cView = resView->GetBufferView()]
            (DxvkContext* ctx) {
              ctx->bindResourceTexelBuffer(cSlotId, cView);
            });
          } else {
            EmitCs([cSlotId = slotId + i, cView = resView->GetImageView()]
            (DxvkContext* ctx) {
              ctx->bindResourceImage(cSlotId, cView);
            });
          }
        } else {
          // When unbinding a resource, it doesn't really matter if
          // the resource type is correct, so we'll just bind a null
          // image to the given resource slot
          EmitCs([cSlotId = slotId + i] (DxvkContext* ctx) {
            ctx->bindResourceImage(cSlotId, nullptr);
          });
        }
      }
    }
  }
  
  
  void D3D11DeviceContext::BindUnorderedAccessViews(
          DxbcProgramType                   ShaderStage,
          D3D11UnorderedAccessBindings&     Bindings,
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews) {
    const uint32_t uavSlotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::UnorderedAccessView,
      StartSlot);
    
    const uint32_t ctrSlotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::UavCounter,
      StartSlot);
    
    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);
      
      if (Bindings[StartSlot + i] != uav) {
        Bindings[StartSlot + i] = uav;
        
        if (uav != nullptr) {
          // Figure out what we have to bind based on the resource type
          if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
            EmitCs([
              cUavSlotId = uavSlotId + i,
              cCtrSlotId = ctrSlotId + i,
              cUavBuffer = uav->GetBufferView(),
              cCtrBuffer = uav->GetCounterSlice()
            ] (DxvkContext* ctx) {
              ctx->bindResourceTexelBuffer(cUavSlotId, cUavBuffer);
              ctx->bindResourceBuffer     (cCtrSlotId, cCtrBuffer);
            });
          } else {
            EmitCs([cUavSlotId = uavSlotId + i, cUavImage = uav->GetImageView()]
            (DxvkContext* ctx) {
              ctx->bindResourceImage(cUavSlotId, cUavImage);
            });
          }
        } else {
          // When unbinding a resource, it doesn't really matter if
          // the resource type is correct, so we'll just bind a null
          // image to the given resource slot
          EmitCs([cUavSlotId = uavSlotId + i, cCtrSlotId = ctrSlotId + i]
          (DxvkContext* ctx) {
            ctx->bindResourceTexelBuffer(cUavSlotId, nullptr);
            ctx->bindResourceBuffer     (cCtrSlotId, DxvkBufferSlice());
          });
        }
      }
    }
  }
  
  
  void D3D11DeviceContext::InitUnorderedAccessViewCounters(
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);
      
      if (uav != nullptr) {
        const DxvkBufferSlice counterSlice = uav->GetCounterSlice();
        const D3D11UavCounter counterValue = { pUAVInitialCounts[i] };
        
        if (counterSlice.defined()
         && counterValue.atomicCtr != 0xFFFFFFFFu) {
          EmitCs([counterSlice, counterValue] (DxvkContext* ctx) {
            ctx->updateBuffer(
              counterSlice.buffer(),
              counterSlice.offset(),
              counterSlice.length(),
              &counterValue);
          });
        }
      }
    }
  }
  
  
  void D3D11DeviceContext::ApplyViewportState() {
    // We cannot set less than one viewport in Vulkan, and
    // rendering with no active viewport is illegal anyway.
    if (m_state.rs.numViewports == 0)
      return;
    
    std::array<VkViewport, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
    std::array<VkRect2D,   D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;
    
    // D3D11's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    for (uint32_t i = 0; i < m_state.rs.numViewports; i++) {
      const D3D11_VIEWPORT& vp = m_state.rs.viewports.at(i);
      
      viewports.at(i) = VkViewport {
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
      // TODO D3D11 docs aren't clear about what should happen
      // when there are undefined scissor rects for a viewport.
      // Figure out what it does on Windows.
      if (enableScissorTest && (i < m_state.rs.numScissors)) {
        const D3D11_RECT& sr = m_state.rs.scissors.at(i);
        
        scissors.at(i) = VkRect2D {
          VkOffset2D { sr.left, sr.top },
          VkExtent2D {
            static_cast<uint32_t>(sr.right  - sr.left),
            static_cast<uint32_t>(sr.bottom - sr.top) } };
      } else {
        scissors.at(i) = VkRect2D {
          VkOffset2D { 0, 0 },
          VkExtent2D {
            D3D11_VIEWPORT_BOUNDS_MAX,
            D3D11_VIEWPORT_BOUNDS_MAX } };
      }
    }
    
    EmitCs([
      cViewportCount = m_state.rs.numViewports,
      cViewports     = viewports,
      cScissors      = scissors
    ] (DxvkContext* ctx) {
      ctx->setViewports(
        cViewportCount,
        cViewports.data(),
        cScissors.data());
    });
  }
  
  
  DxvkDataSlice D3D11DeviceContext::AllocUpdateBufferSlice(size_t Size) {
    constexpr size_t UpdateBufferSize = 4 * 1024 * 1024;
    
    if (Size >= UpdateBufferSize) {
      Rc<DxvkDataBuffer> buffer = new DxvkDataBuffer(Size);
      return buffer->alloc(Size);
    } else {
      if (m_updateBuffer == nullptr)
        m_updateBuffer = new DxvkDataBuffer(Size);
      
      DxvkDataSlice slice = m_updateBuffer->alloc(Size);
      
      if (slice.ptr() == nullptr) {
        m_updateBuffer = new DxvkDataBuffer(Size);
        slice = m_updateBuffer->alloc(Size);
      }
      
      return slice;
    }
  }
  
}
