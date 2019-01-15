#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_query.h"
#include "d3d11_texture.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {
  
  D3D11DeviceContext::D3D11DeviceContext(
          D3D11Device*            pParent,
    const Rc<DxvkDevice>&         Device,
          DxvkCsChunkFlags        CsFlags)
  : m_parent    (pParent),
    m_annotation(this),
    m_multithread(this, false),
    m_device    (Device),
    m_csFlags   (CsFlags),
    m_csChunk   (AllocCsChunk()),
    m_cmdData   (nullptr) {
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
  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11DeviceContext)
     || riid == __uuidof(ID3D11DeviceContext1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3DUserDefinedAnnotation)) {
      *ppvObject = ref(&m_annotation);
      return S_OK;
    }

    if (riid == __uuidof(ID3D10Multithread)) {
      *ppvObject = ref(&m_multithread);
      return S_OK;
    }
  
    Logger::warn("D3D11DeviceContext::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  

  void STDMETHODCALLTYPE D3D11DeviceContext::DiscardResource(ID3D11Resource* pResource) {
    D3D10DeviceLock lock = LockContext();

    if (!pResource)
      return;
    
    // We don't support the Discard API for images
    D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resType);

    if (resType == D3D11_RESOURCE_DIMENSION_BUFFER)
      DiscardBuffer(static_cast<D3D11Buffer*>(pResource));
    else if (resType != D3D11_RESOURCE_DIMENSION_UNKNOWN)
      DiscardTexture(GetCommonTexture(pResource));
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView(ID3D11View* pResourceView) {
    D3D10DeviceLock lock = LockContext();

    // ID3D11View has no methods to query the exact type of
    // the view, so we'll have to check each possible class
    auto dsv = dynamic_cast<D3D11DepthStencilView*>(pResourceView);
    auto rtv = dynamic_cast<D3D11RenderTargetView*>(pResourceView);
    auto uav = dynamic_cast<D3D11UnorderedAccessView*>(pResourceView);

    Rc<DxvkImageView> view;
    if (dsv) view = dsv->GetImageView();
    if (rtv) view = rtv->GetImageView();
    if (uav) view = uav->GetImageView();

    if (view != nullptr) {
      EmitCs([cView = std::move(view)]
      (DxvkContext* ctx) {
        ctx->discardImage(
          cView->image(),
          cView->subresources());
      });
    }
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView1(
          ID3D11View*              pResourceView, 
    const D3D11_RECT*              pRects, 
          UINT                     NumRects) {
    static bool s_errorShown = false;
    
    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::DiscardView1: Not implemented");
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::SwapDeviceContextState(
          ID3DDeviceContextState*  pState, 
          ID3DDeviceContextState** ppPreviousState) {
    Logger::err("D3D11DeviceContext::SwapDeviceContextState: Not implemented");
  }
  

  void STDMETHODCALLTYPE D3D11DeviceContext::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_parent);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearState() {
    D3D10DeviceLock lock = LockContext();

    // Default shaders
    m_state.vs.shader = nullptr;
    m_state.hs.shader = nullptr;
    m_state.ds.shader = nullptr;
    m_state.gs.shader = nullptr;
    m_state.ps.shader = nullptr;
    m_state.cs.shader = nullptr;
    
    // Default constant buffers
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
      m_state.vs.constantBuffers[i] = { nullptr, 0, 0 };
      m_state.hs.constantBuffers[i] = { nullptr, 0, 0 };
      m_state.ds.constantBuffers[i] = { nullptr, 0, 0 };
      m_state.gs.constantBuffers[i] = { nullptr, 0, 0 };
      m_state.ps.constantBuffers[i] = { nullptr, 0, 0 };
      m_state.cs.constantBuffers[i] = { nullptr, 0, 0 };
    }
    
    // Default samplers
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
      m_state.vs.samplers[i] = nullptr;
      m_state.hs.samplers[i] = nullptr;
      m_state.ds.samplers[i] = nullptr;
      m_state.gs.samplers[i] = nullptr;
      m_state.ps.samplers[i] = nullptr;
      m_state.cs.samplers[i] = nullptr;
    }
    
    // Default shader resources
    for (uint32_t i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
      m_state.vs.shaderResources[i] = nullptr;
      m_state.hs.shaderResources[i] = nullptr;
      m_state.ds.shaderResources[i] = nullptr;
      m_state.gs.shaderResources[i] = nullptr;
      m_state.ps.shaderResources[i] = nullptr;
      m_state.cs.shaderResources[i] = nullptr;
    }
    
    // Default UAVs
    for (uint32_t i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++) {
      m_state.ps.unorderedAccessViews[i] = nullptr;
      m_state.cs.unorderedAccessViews[i] = nullptr;
    }

    // Default ID state
    m_state.id.argBuffer = nullptr;
    
    // Default IA state
    m_state.ia.inputLayout       = nullptr;
    m_state.ia.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    
    for (uint32_t i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
      m_state.ia.vertexBuffers[i].buffer = nullptr;
      m_state.ia.vertexBuffers[i].offset = 0;
      m_state.ia.vertexBuffers[i].stride = 0;
    }
    
    m_state.ia.indexBuffer.buffer = nullptr;
    m_state.ia.indexBuffer.offset = 0;
    m_state.ia.indexBuffer.format = DXGI_FORMAT_UNKNOWN;
    
    // Default OM State
    for (uint32_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      m_state.om.renderTargetViews[i] = nullptr;
    m_state.om.depthStencilView = nullptr;
    
    m_state.om.cbState = nullptr;
    m_state.om.dsState = nullptr;
    
    for (uint32_t i = 0; i < 4; i++)
      m_state.om.blendFactor[i] = 0.0f;
    
    m_state.om.sampleMask = D3D11_DEFAULT_SAMPLE_MASK;
    m_state.om.stencilRef = D3D11_DEFAULT_STENCIL_REFERENCE;
    
    // Default RS state
    m_state.rs.state        = nullptr;
    m_state.rs.numViewports = 0;
    m_state.rs.numScissors  = 0;
    
    for (uint32_t i = 0; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++) {
      m_state.rs.viewports[i] = D3D11_VIEWPORT { };
      m_state.rs.scissors [i] = D3D11_RECT     { };
    }
    
    // Default SO state
    for (uint32_t i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++) {
      m_state.so.targets[i].buffer = nullptr;
      m_state.so.targets[i].offset = 0;
    }
    
    // Default predication
    m_state.pr.predicateObject = nullptr;
    m_state.pr.predicateValue  = FALSE;
    
    // Make sure to apply all state
    RestoreState();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (!pAsync)
      return;
    
    Com<D3D11Query> queryPtr = static_cast<D3D11Query*>(pAsync);
      
    if (queryPtr->HasBeginEnabled()) {
      uint32_t revision = queryPtr->Reset();
      EmitCs([revision, queryPtr] (DxvkContext* ctx) {
        queryPtr->Begin(ctx, revision);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::End(ID3D11Asynchronous *pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (!pAsync)
      return;
    
    Com<D3D11Query> queryPtr = static_cast<D3D11Query*>(pAsync);
    
    if (queryPtr->HasBeginEnabled()) {
      EmitCs([queryPtr] (DxvkContext* ctx) {
        queryPtr->End(ctx);
      });
    } else {
      uint32_t revision = queryPtr->Reset();
      EmitCs([revision, queryPtr] (DxvkContext* ctx) {
        queryPtr->Signal(ctx, revision);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetPredication(
          ID3D11Predicate*                  pPredicate,
          BOOL                              PredicateValue) {
    static bool s_errorShown = false;
    
    if (pPredicate && !std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetPredication: Stub");
    
    D3D10DeviceLock lock = LockContext();
    
    m_state.pr.predicateObject = static_cast<D3D11Query*>(pPredicate);
    m_state.pr.predicateValue  = PredicateValue;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GetPredication(
          ID3D11Predicate**                 ppPredicate,
          BOOL*                             pPredicateValue) {
    D3D10DeviceLock lock = LockContext();

    if (ppPredicate != nullptr)
      *ppPredicate = m_state.pr.predicateObject.ref();
    
    if (pPredicateValue != nullptr)
      *pPredicateValue = m_state.pr.predicateValue;
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
    CopySubresourceRegion1(
      pDstResource, DstSubresource, DstX, DstY, DstZ,
      pSrcResource, SrcSubresource, pSrcBox, 0);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CopySubresourceRegion1(
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

    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    
    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);
    
    // Copying 2D image slices to 3D images and vice versa is legal
    const bool copy2Dto3D = dstResourceDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D
                         && srcResourceDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    const bool copy3Dto2D = dstResourceDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D
                         && srcResourceDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D;
    
    if (dstResourceDim != srcResourceDim && !copy2Dto3D && !copy3Dto2D) {
      Logger::err(str::format(
        "D3D11: CopySubresourceRegion: Incompatible resources",
        "\n  Dst resource type: ", dstResourceDim,
        "\n  Src resource type: ", srcResourceDim));
      return;
    }
    
    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource)->GetBufferSlice();
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource)->GetBufferSlice();

      if (CopyFlags & D3D11_COPY_DISCARD)
        DiscardBuffer(static_cast<D3D11Buffer*>(pDstResource));
      
      VkDeviceSize dstOffset = DstX;
      VkDeviceSize srcOffset = 0;
      VkDeviceSize regLength = srcBuffer.length();
      
      if (dstOffset >= dstBuffer.length())
        return;
      
      if (pSrcBox != nullptr) {
        if (pSrcBox->left >= pSrcBox->right)
          return;  // no-op, but legal
        
        srcOffset = pSrcBox->left;
        regLength = pSrcBox->right - pSrcBox->left;
        
        if (srcOffset >= srcBuffer.length())
          return;
      }
      
      // Clamp copy region to prevent out-of-bounds access
      regLength = std::min(regLength, srcBuffer.length() - srcOffset);
      regLength = std::min(regLength, dstBuffer.length() - dstOffset);
      
      EmitCs([
        cDstSlice = dstBuffer.subSlice(dstOffset, regLength),
        cSrcSlice = srcBuffer.subSlice(srcOffset, regLength)
      ] (DxvkContext* ctx) {
        bool sameResource = cDstSlice.buffer() == cSrcSlice.buffer();

        if (!sameResource) {
          ctx->copyBuffer(
            cDstSlice.buffer(),
            cDstSlice.offset(),
            cSrcSlice.buffer(),
            cSrcSlice.offset(),
            cSrcSlice.length());
        } else {
          ctx->copyBufferRegion(
            cDstSlice.buffer(),
            cDstSlice.offset(),
            cSrcSlice.offset(),
            cSrcSlice.length());
        }
      });
    } else {
      const D3D11CommonTexture* dstTextureInfo = GetCommonTexture(pDstResource);
      const D3D11CommonTexture* srcTextureInfo = GetCommonTexture(pSrcResource);
      
      const Rc<DxvkImage> dstImage = dstTextureInfo->GetImage();
      const Rc<DxvkImage> srcImage = srcTextureInfo->GetImage();
      
      const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
      const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);
      
      const VkImageSubresource dstSubresource = dstTextureInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, DstSubresource);
      const VkImageSubresource srcSubresource = srcTextureInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, SrcSubresource);
      
      // Copies are only supported on size-compatible formats
      if (dstFormatInfo->elementSize != srcFormatInfo->elementSize) {
        Logger::err(str::format(
          "D3D11: CopySubresourceRegion: Incompatible texel size"
          "\n  Dst texel size: ", dstFormatInfo->elementSize,
          "\n  Src texel size: ", srcFormatInfo->elementSize));
        return;
      }
      
      // Copies are only supported if the sample count matches
      if (dstImage->info().sampleCount != srcImage->info().sampleCount) {
        Logger::err(str::format(
          "D3D11: CopySubresourceRegion: Incompatible sample count",
          "\n  Dst sample count: ", dstImage->info().sampleCount,
          "\n  Src sample count: ", srcImage->info().sampleCount));
        return;
      }
      
      VkOffset3D srcOffset = { 0, 0, 0 };
      VkOffset3D dstOffset = { int32_t(DstX), int32_t(DstY), int32_t(DstZ) };
      
      VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);
      VkExtent3D dstExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);
      VkExtent3D regExtent = srcExtent;
      
      if (uint32_t(dstOffset.x) >= dstExtent.width
       || uint32_t(dstOffset.y) >= dstExtent.height
       || uint32_t(dstOffset.z) >= dstExtent.depth)
        return;
      
      if (pSrcBox != nullptr) {
        if (pSrcBox->left  >= pSrcBox->right
         || pSrcBox->top   >= pSrcBox->bottom
         || pSrcBox->front >= pSrcBox->back)
          return;  // no-op, but legal
        
        srcOffset.x = pSrcBox->left;
        srcOffset.y = pSrcBox->top;
        srcOffset.z = pSrcBox->front;
        
        regExtent.width  = pSrcBox->right -  pSrcBox->left;
        regExtent.height = pSrcBox->bottom - pSrcBox->top;
        regExtent.depth  = pSrcBox->back -   pSrcBox->front;
        
        if (uint32_t(srcOffset.x) >= srcExtent.width
         || uint32_t(srcOffset.y) >= srcExtent.height
         || uint32_t(srcOffset.z) >= srcExtent.depth)
          return;
      }
      
      VkImageSubresourceLayers dstLayers = {
        dstSubresource.aspectMask,
        dstSubresource.mipLevel,
        dstSubresource.arrayLayer, 1 };
      
      VkImageSubresourceLayers srcLayers = {
        srcSubresource.aspectMask,
        srcSubresource.mipLevel,
        srcSubresource.arrayLayer, 1 };
      
      // Copying multiple slices does not
      // seem to be supported in D3D11
      if (copy2Dto3D || copy3Dto2D) {
        regExtent.depth      = 1;
        dstLayers.layerCount = 1;
        srcLayers.layerCount = 1;
      }
      
      // Don't perform the copy if the offsets aren't aligned
      if (!util::isBlockAligned(srcOffset, srcFormatInfo->blockSize)
       || !util::isBlockAligned(dstOffset, dstFormatInfo->blockSize)) {
        Logger::err(str::format(
          "D3D11: CopySubresourceRegion: Unaligned block offset",
          "\n  Src offset:     (", srcOffset.x,     ",", srcOffset.y,      ",", srcOffset.z,     ")",
          "\n  Src block size: (", srcFormatInfo->blockSize.width, "x", srcFormatInfo->blockSize.height, "x", srcFormatInfo->blockSize.depth, ")",
          "\n  Dst offset:     (", dstOffset.x,     ",", dstOffset.y,      ",", dstOffset.z,     ")",
          "\n  Dst block size: (", dstFormatInfo->blockSize.width, "x", dstFormatInfo->blockSize.height, "x", dstFormatInfo->blockSize.depth, ")"));
        return;
      }
      
      // Clamp the image region in order to avoid out-of-bounds access
      VkExtent3D regBlockCount = util::computeBlockCount(regExtent, srcFormatInfo->blockSize);
      VkExtent3D dstBlockCount = util::computeMaxBlockCount(dstOffset, dstExtent, dstFormatInfo->blockSize);
      VkExtent3D srcBlockCount = util::computeMaxBlockCount(srcOffset, srcExtent, srcFormatInfo->blockSize);
      
      regBlockCount = util::minExtent3D(regBlockCount, dstBlockCount);
      regBlockCount = util::minExtent3D(regBlockCount, srcBlockCount);
      
      regExtent = util::minExtent3D(regExtent, util::computeBlockExtent(regBlockCount, srcFormatInfo->blockSize));
      
      // Don't perform the copy if the image extent is not aligned and
      // if it does not touch the image border for unaligned dimensons
      if (!util::isBlockAligned(srcOffset, regExtent, srcFormatInfo->blockSize, srcExtent)) {
        Logger::err(str::format(
          "D3D11: CopySubresourceRegion: Unaligned block size",
          "\n  Src offset:     (", srcOffset.x,     ",", srcOffset.y,      ",", srcOffset.z,     ")",
          "\n  Src extent:     (", srcExtent.width, "x", srcExtent.height, "x", srcExtent.depth, ")",
          "\n  Src block size: (", srcFormatInfo->blockSize.width, "x", srcFormatInfo->blockSize.height, "x", srcFormatInfo->blockSize.depth, ")",
          "\n  Dst offset:     (", dstOffset.x,     ",", dstOffset.y,      ",", dstOffset.z,     ")",
          "\n  Dst extent:     (", dstExtent.width, "x", dstExtent.height, "x", dstExtent.depth, ")",
          "\n  Dst block size: (", dstFormatInfo->blockSize.width, "x", dstFormatInfo->blockSize.height, "x", dstFormatInfo->blockSize.depth, ")",
          "\n  Region extent:  (", regExtent.width, "x", regExtent.height, "x", regExtent.depth, ")"));
        return;
      }
      
      EmitCs([
        cDstImage  = dstImage,
        cSrcImage  = srcImage,
        cDstLayers = dstLayers,
        cSrcLayers = srcLayers,
        cDstOffset = dstOffset,
        cSrcOffset = srcOffset,
        cExtent    = regExtent
      ] (DxvkContext* ctx) {
        bool sameSubresource = cDstImage  == cSrcImage
                            && cDstLayers == cSrcLayers;
        
        if (!sameSubresource) {
          ctx->copyImage(
            cDstImage, cDstLayers, cDstOffset,
            cSrcImage, cSrcLayers, cSrcOffset,
            cExtent);
        } else {
          ctx->copyImageRegion(
            cDstImage, cDstLayers,
            cDstOffset, cSrcOffset,
            cExtent);
        }
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    D3D10DeviceLock lock = LockContext();

    if (!pDstResource || !pSrcResource || (pDstResource == pSrcResource))
      return;
    
    D3D11_RESOURCE_DIMENSION dstResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_RESOURCE_DIMENSION srcResourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    
    pDstResource->GetType(&dstResourceDim);
    pSrcResource->GetType(&srcResourceDim);
    
    if (dstResourceDim != srcResourceDim) {
      Logger::err(str::format(
        "D3D11: CopyResource: Incompatible resources",
        "\n  Dst resource type: ", dstResourceDim,
        "\n  Src resource type: ", srcResourceDim));
      return;
    }
    
    if (dstResourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto dstBuffer = static_cast<D3D11Buffer*>(pDstResource)->GetBufferSlice();
      auto srcBuffer = static_cast<D3D11Buffer*>(pSrcResource)->GetBufferSlice();
      
      if (dstBuffer.length() != srcBuffer.length()) {
        Logger::err(str::format(
          "D3D11: CopyResource: Mismatched buffer size",
          "\n  Dst buffer size: ", dstBuffer.length(),
          "\n  Src buffer size: ", srcBuffer.length()));
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
      const Rc<DxvkImage> dstImage = GetCommonTexture(pDstResource)->GetImage();
      const Rc<DxvkImage> srcImage = GetCommonTexture(pSrcResource)->GetImage();

      const DxvkFormatInfo* dstFormatInfo = imageFormatInfo(dstImage->info().format);
      const DxvkFormatInfo* srcFormatInfo = imageFormatInfo(srcImage->info().format);

      // Copies are only supported on size-compatible formats
      if (dstFormatInfo->elementSize != srcFormatInfo->elementSize) {
        Logger::err(str::format(
          "D3D11: CopyResource: Incompatible texel size"
          "\n  Dst texel size: ", dstFormatInfo->elementSize,
          "\n  Src texel size: ", srcFormatInfo->elementSize));
        return;
      }

      // Layer count, mip level count, and sample count must match
      if (srcImage->info().numLayers != dstImage->info().numLayers
       || srcImage->info().mipLevels != dstImage->info().mipLevels
       || srcImage->info().sampleCount != dstImage->info().sampleCount) {
        Logger::err(str::format(
          "D3D11: CopyResource: Incompatible images"
          "\n  Dst: (", dstImage->info().numLayers,
                   ",", dstImage->info().mipLevels,
                   ",", dstImage->info().sampleCount, ")",
          "\n  Src: (", srcImage->info().numLayers,
                   ",", srcImage->info().mipLevels,
                   ",", srcImage->info().sampleCount, ")"));
        return;
      }
      
      for (uint32_t i = 0; i < srcImage->info().mipLevels; i++) {
        VkImageSubresourceLayers dstLayers = { dstFormatInfo->aspectMask, i, 0, dstImage->info().numLayers };
        VkImageSubresourceLayers srcLayers = { srcFormatInfo->aspectMask, i, 0, srcImage->info().numLayers };
        
        VkExtent3D extent = srcImage->mipLevelExtent(i);
        
        EmitCs([
          cDstImage  = dstImage,
          cSrcImage  = srcImage,
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
    D3D10DeviceLock lock = LockContext();

    auto buf = static_cast<D3D11Buffer*>(pDstBuffer);
    auto uav = static_cast<D3D11UnorderedAccessView*>(pSrcView);

    if (!buf || !uav)
      return;

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
    D3D10DeviceLock lock = LockContext();

    auto rtv = static_cast<D3D11RenderTargetView*>(pRenderTargetView);
    
    if (!rtv)
      return;
    
    const Rc<DxvkImageView> view = rtv->GetImageView();
    
    VkClearValue clearValue;
    clearValue.color.float32[0] = ColorRGBA[0];
    clearValue.color.float32[1] = ColorRGBA[1];
    clearValue.color.float32[2] = ColorRGBA[2];
    clearValue.color.float32[3] = ColorRGBA[3];
    
    EmitCs([
      cClearValue = clearValue,
      cImageView  = view
    ] (DxvkContext* ctx) {
      ctx->clearRenderTarget(
        cImageView,
        VK_IMAGE_ASPECT_COLOR_BIT,
        cClearValue);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewUint(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const UINT                              Values[4]) {
    D3D10DeviceLock lock = LockContext();

    auto uav = static_cast<D3D11UnorderedAccessView*>(pUnorderedAccessView);
    
    if (!uav)
      return;
    
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
    
    // Set up clear color struct
    VkClearValue clearValue;
    clearValue.color.uint32[0] = Values[0];
    clearValue.color.uint32[1] = Values[1];
    clearValue.color.uint32[2] = Values[2];
    clearValue.color.uint32[3] = Values[3];

    // This is the only packed format that has UAV support
    if (uavFormat == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
      clearValue.color.uint32[0] = ((Values[0] & 0x7FF) <<  0)
                                 | ((Values[1] & 0x7FF) << 11)
                                 | ((Values[2] & 0x3FF) << 22);
    }
    
    if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
      // In case of raw and structured buffers as well as typed
      // buffers that can be used for atomic operations, we can
      // use the fast Vulkan buffer clear function.
      Rc<DxvkBufferView> bufferView = uav->GetBufferView();
      
      if (bufferView->info().format == VK_FORMAT_R32_UINT
       || bufferView->info().format == VK_FORMAT_R32_SINT
       || bufferView->info().format == VK_FORMAT_R32_SFLOAT) {
        EmitCs([
          cClearValue = Values[0],
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
      // Create a view with an integer format if necessary
      Rc<DxvkImageView> imageView = uav->GetImageView();
      
      if (uavFormat != rawFormat) {
        DxvkImageViewCreateInfo info = imageView->info();
        info.format = rawFormat;
        
        imageView = m_device->createImageView(
          imageView->image(), info);
      }
      
      EmitCs([
        cClearValue = clearValue,
        cDstView    = imageView
      ] (DxvkContext* ctx) {
        ctx->clearImageView(cDstView,
          VkOffset3D { 0, 0, 0 },
          cDstView->mipLevelExtent(0),
          cClearValue);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewFloat(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const FLOAT                             Values[4]) {
    D3D10DeviceLock lock = LockContext();

    auto uav = static_cast<D3D11UnorderedAccessView*>(pUnorderedAccessView);
    
    if (!uav)
      return;
    
    VkClearValue clearValue;
    clearValue.color.float32[0] = Values[0];
    clearValue.color.float32[1] = Values[1];
    clearValue.color.float32[2] = Values[2];
    clearValue.color.float32[3] = Values[3];
    
    if (uav->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
      EmitCs([
        cClearValue = clearValue,
        cDstView    = uav->GetBufferView()
      ] (DxvkContext* ctx) {
        ctx->clearBufferView(
          cDstView, 0,
          cDstView->elementCount(),
          cClearValue.color);
      });
    } else {
      EmitCs([
        cClearValue = clearValue,
        cDstView    = uav->GetImageView()
      ] (DxvkContext* ctx) {
        ctx->clearImageView(cDstView,
          VkOffset3D { 0, 0, 0 },
          cDstView->mipLevelExtent(0),
          cClearValue);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearDepthStencilView(
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    D3D10DeviceLock lock = LockContext();

    auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
    
    if (!dsv)
      return;
    
    // Figure out which aspects to clear based
    // on the image format and the clear flags.
    const Rc<DxvkImageView> view = dsv->GetImageView();
    
    VkImageAspectFlags aspectMask = 0;
    
    if (ClearFlags & D3D11_CLEAR_DEPTH)
      aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    
    if (ClearFlags & D3D11_CLEAR_STENCIL)
      aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    
    aspectMask &= imageFormatInfo(view->info().format)->aspectMask;
    
    VkClearValue clearValue;
    clearValue.depthStencil.depth   = Depth;
    clearValue.depthStencil.stencil = Stencil;
    
    EmitCs([
      cClearValue = clearValue,
      cAspectMask = aspectMask,
      cImageView  = view
    ] (DxvkContext* ctx) {
      ctx->clearRenderTarget(
        cImageView,
        cAspectMask,
        cClearValue);
    });
  }
  

  void STDMETHODCALLTYPE D3D11DeviceContext::ClearView(
          ID3D11View*                       pView, 
    const FLOAT                             Color[4], 
    const D3D11_RECT*                       pRect, 
          UINT                              NumRects) {
    D3D10DeviceLock lock = LockContext();

    // ID3D11View has no methods to query the exact type of
    // the view, so we'll have to check each possible class
    auto dsv = dynamic_cast<D3D11DepthStencilView*>(pView);
    auto rtv = dynamic_cast<D3D11RenderTargetView*>(pView);
    auto uav = dynamic_cast<D3D11UnorderedAccessView*>(pView);

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
    const DxvkFormatInfo* formatInfo = imageFormatInfo(format);

    // Convert the clear color format. ClearView takes
    // the clear value for integer formats as a set of
    // integral floats, so we'll have to convert.
    VkClearValue clearValue;

    if (imgView == nullptr || imgView->info().aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
      for (uint32_t i = 0; i < 4; i++) {
        if (formatInfo->flags.test(DxvkFormatFlag::SampledUInt))
          clearValue.color.uint32[i] = uint32_t(Color[i]);
        else if (formatInfo->flags.test(DxvkFormatFlag::SampledSInt))
          clearValue.color.int32[i] = int32_t(Color[i]);
        else
          clearValue.color.float32[i] = Color[i];
      }
    } else {
      clearValue.depthStencil.depth   = Color[0];
      clearValue.depthStencil.stencil = 0;
    }

    // Clear all the rectangles that are specified
    for (uint32_t i = 0; i < NumRects; i++) {
      if (pRect[i].left >= pRect[i].right
       || pRect[i].top >= pRect[i].bottom)
        continue;
      
      if (bufView != nullptr) {
        VkDeviceSize offset = pRect[i].left;
        VkDeviceSize length = pRect[i].right - pRect[i].left;

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
        VkOffset3D offset = { pRect[i].left, pRect[i].top, 0 };
        VkExtent3D extent = { 
          uint32_t(pRect[i].right - pRect[i].left),
          uint32_t(pRect[i].bottom - pRect[i].top), 1 };
        
        EmitCs([
          cImageView    = imgView,
          cAreaOffset   = offset,
          cAreaExtent   = extent,
          cClearValue   = clearValue
        ] (DxvkContext* ctx) {
          ctx->clearImageView(
            cImageView,
            cAreaOffset,
            cAreaExtent,
            cClearValue);
        });
      }
    }

    // The rect array is optional, so if it is not
    // specified, we'll have to clear the entire view
    if (pRect == nullptr) {
      if (bufView != nullptr) {
        EmitCs([
          cBufferView   = bufView,
          cClearValue   = clearValue,
          cElementSize  = formatInfo->elementSize
        ] (DxvkContext* ctx) {
          ctx->clearBufferView(cBufferView,
            cBufferView->info().rangeOffset / cElementSize,
            cBufferView->info().rangeLength / cElementSize,
            cClearValue.color);
        });
      }

      if (imgView != nullptr) {
        EmitCs([
          cImageView    = imgView,
          cClearValue   = clearValue
        ] (DxvkContext* ctx) {
          VkOffset3D offset = { 0, 0, 0 };
          VkExtent3D extent = cImageView->mipLevelExtent(0);

          ctx->clearImageView(cImageView,
            offset, extent, cClearValue);
        });
      }
    }
  }
  

  void STDMETHODCALLTYPE D3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) {
    D3D10DeviceLock lock = LockContext();

    auto view = static_cast<D3D11ShaderResourceView*>(pShaderResourceView);

    if (!view || view->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER)
      return;
      
    EmitCs([cDstImageView = view->GetImageView()]
    (DxvkContext* ctx) {
      ctx->generateMipmaps(cDstImageView);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    UpdateSubresource1(pDstResource,
      DstSubresource, pDstBox, pSrcData,
      SrcRowPitch, SrcDepthPitch, 0);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::UpdateSubresource1(
          ID3D11Resource*                   pDstResource, 
          UINT                              DstSubresource, 
    const D3D11_BOX*                        pDstBox, 
    const void*                             pSrcData, 
          UINT                              SrcRowPitch, 
          UINT                              SrcDepthPitch, 
          UINT                              CopyFlags) {
    D3D10DeviceLock lock = LockContext();

    if (!pDstResource)
      return;
    
    // We need a different code path for buffers
    D3D11_RESOURCE_DIMENSION resourceType;
    pDstResource->GetType(&resourceType);
    
    if (resourceType == D3D11_RESOURCE_DIMENSION_BUFFER) {
      const auto bufferResource = static_cast<D3D11Buffer*>(pDstResource);
      const auto bufferSlice = bufferResource->GetBufferSlice();

      if (CopyFlags & D3D11_COPY_DISCARD)
        DiscardBuffer(bufferResource);
      
      VkDeviceSize offset = bufferSlice.offset();
      VkDeviceSize size   = bufferSlice.length();
      
      if (pDstBox != nullptr) {
        offset = pDstBox->left;
        size   = pDstBox->right - pDstBox->left;
      }
      
      if (offset + size > bufferSlice.length()) {
        Logger::err(str::format(
          "D3D11: UpdateSubresource: Buffer update range out of bounds",
          "\n  Dst slice offset: ", bufferSlice.offset(),
          "\n  Dst slice length: ", bufferSlice.length(),
          "\n  Src slice offset: ", offset,
          "\n  Src slice length: ", size));
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
      const D3D11CommonTexture* textureInfo = GetCommonTexture(pDstResource);
      
      const VkImageSubresource subresource =
        textureInfo->GetSubresourceFromIndex(
          VK_IMAGE_ASPECT_COLOR_BIT, DstSubresource);
      
      VkOffset3D offset = { 0, 0, 0 };
      VkExtent3D extent = textureInfo->GetImage()->mipLevelExtent(subresource.mipLevel);
      
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
        textureInfo->GetImage()->info().format);
      
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
        cDstImage         = textureInfo->GetImage(),
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
     || srcResourceType != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::err(str::format(
        "D3D11: ResolveSubresource: Incompatible resources",
        "\n  Dst resource type: ", dstResourceType,
        "\n  Src resource type: ", srcResourceType));
      return;
    }
    
    auto dstTexture = static_cast<D3D11Texture2D*>(pDstResource);
    auto srcTexture = static_cast<D3D11Texture2D*>(pSrcResource);
    
    D3D11_TEXTURE2D_DESC dstDesc;
    D3D11_TEXTURE2D_DESC srcDesc;
    
    dstTexture->GetDesc(&dstDesc);
    srcTexture->GetDesc(&srcDesc);
    
    if (dstDesc.SampleDesc.Count != 1) {
      Logger::err(str::format(
        "D3D11: ResolveSubresource: Invalid sample counts",
        "\n  Dst sample count: ", dstDesc.SampleDesc.Count,
        "\n  Src sample count: ", srcDesc.SampleDesc.Count));
      return;
    }
    
    const D3D11CommonTexture* dstTextureInfo = GetCommonTexture(pDstResource);
    const D3D11CommonTexture* srcTextureInfo = GetCommonTexture(pSrcResource);
    
    const DXGI_VK_FORMAT_INFO dstFormatInfo = m_parent->LookupFormat(dstDesc.Format, DXGI_VK_FORMAT_MODE_ANY);
    const DXGI_VK_FORMAT_INFO srcFormatInfo = m_parent->LookupFormat(srcDesc.Format, DXGI_VK_FORMAT_MODE_ANY);
    
    auto dstVulkanFormatInfo = imageFormatInfo(dstFormatInfo.Format);
    auto srcVulkanFormatInfo = imageFormatInfo(srcFormatInfo.Format);
    
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
    
    if (srcDesc.SampleDesc.Count == 1) {
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
        ctx->resolveImage(
          cDstImage, cDstSubres,
          cSrcImage, cSrcSubres,
          cFormat);
      });
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawAuto() {
    D3D10DeviceLock lock = LockContext();

    D3D11Buffer* buffer = m_state.ia.vertexBuffers[0].buffer.ptr();

    if (buffer == nullptr)
      return;
    
    DxvkBufferSlice vtxBuf = buffer->GetBufferSlice();
    DxvkBufferSlice ctrBuf = buffer->GetSOCounter();

    if (!ctrBuf.defined())
      return;

    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndirectXfb(ctrBuf,
        vtxBuf.buffer()->getXfbVertexStride(),
        0); // FIXME offset?
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Draw(
          UINT            VertexCount,
          UINT            StartVertexLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCount, 1,
        StartVertexLocation, 0);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexed(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstanced(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstanced(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    
    SetDrawBuffer(pBufferForArgs);
    
    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    constexpr VkDeviceSize stride = sizeof(VkDrawIndexedIndirectCommand);
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);

    bool useMultiDraw = cmdData && cmdData->type == D3D11CmdType::DrawIndirectIndexed
      && cmdData->offset + cmdData->count * stride == AlignedByteOffsetForArgs
      && m_device->features().core.features.multiDrawIndirect;
    
    if (useMultiDraw) {
      cmdData->count += 1;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndexedIndirect(data->offset, data->count,
            sizeof(VkDrawIndexedIndirectCommand));
        });
      
      cmdData->type   = D3D11CmdType::DrawIndirectIndexed;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    
    SetDrawBuffer(pBufferForArgs);

    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    constexpr VkDeviceSize stride = sizeof(VkDrawIndirectCommand);
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);

    bool useMultiDraw = cmdData && cmdData->type == D3D11CmdType::DrawIndirect
      && cmdData->offset + cmdData->count * stride == AlignedByteOffsetForArgs
      && m_device->features().core.features.multiDrawIndirect;
    
    if (useMultiDraw) {
      cmdData->count += 1;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndirect(data->offset, data->count,
            sizeof(VkDrawIndirectCommand));
        });
      
      cmdData->type   = D3D11CmdType::DrawIndirect;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Dispatch(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DispatchIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    
    SetDrawBuffer(pBufferForArgs);
    
    EmitCs([cOffset = AlignedByteOffsetForArgs]
    (DxvkContext* ctx) {
      ctx->dispatchIndirect(cOffset);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    D3D10DeviceLock lock = LockContext();
    
    if (m_state.ia.primitiveTopology != Topology) {
      m_state.ia.primitiveTopology = Topology;
      ApplyPrimitiveTopology();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppVertexBuffers[i]);
      
      m_state.ia.vertexBuffers[StartSlot + i].buffer = newBuffer;
      m_state.ia.vertexBuffers[StartSlot + i].offset = pOffsets[i];
      m_state.ia.vertexBuffers[StartSlot + i].stride = pStrides[i];
      
      BindVertexBuffer(StartSlot + i, newBuffer, pOffsets[i], pStrides[i]);
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    D3D10DeviceLock lock = LockContext();
    
    auto newBuffer = static_cast<D3D11Buffer*>(pIndexBuffer);
    
    m_state.ia.indexBuffer.buffer = newBuffer;
    m_state.ia.indexBuffer.offset = Offset;
    m_state.ia.indexBuffer.format = Format;
    
    BindIndexBuffer(newBuffer, Offset, Format);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) {
    D3D10DeviceLock lock = LockContext();
    
    *ppInputLayout = m_state.ia.inputLayout.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) {
    D3D10DeviceLock lock = LockContext();
    
    *pTopology = m_state.ia.primitiveTopology;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IAGetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppVertexBuffers,
          UINT*                             pStrides,
          UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();
    
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
    D3D10DeviceLock lock = LockContext();
    
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
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11VertexShader*>(pVertexShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.vs.shader != shader) {
      m_state.vs.shader = shader;

      BindShader(
        DxbcProgramType::VertexShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::VertexShader,
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::VertexShader,
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::VertexShader,
      m_state.vs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
      DxbcProgramType::VertexShader,
      m_state.vs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShader(
          ID3D11VertexShader**              ppVertexShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppVertexShader != nullptr)
      *ppVertexShader = m_state.vs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer**                    ppConstantBuffers, 
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.vs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.vs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShader(
          ID3D11HullShader*                 pHullShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11HullShader*>(pHullShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.hs.shader != shader) {
      m_state.hs.shader = shader;

      BindShader(
        DxbcProgramType::HullShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::HullShader,
      m_state.hs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::HullShader,
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::HullShader,
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
      DxbcProgramType::HullShader,
      m_state.hs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShader(
          ID3D11HullShader**                ppHullShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppHullShader != nullptr)
      *ppHullShader = m_state.hs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void D3D11DeviceContext::HSGetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer**                    ppConstantBuffers, 
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.hs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.hs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShader(
          ID3D11DomainShader*               pDomainShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11DomainShader*>(pDomainShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.ds.shader != shader) {
      m_state.ds.shader = shader;

      BindShader(
        DxbcProgramType::DomainShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::DomainShader,
      m_state.ds.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::DomainShader,
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::DomainShader,
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
      DxbcProgramType::DomainShader,
      m_state.ds.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShader(
          ID3D11DomainShader**              ppDomainShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppDomainShader != nullptr)
      *ppDomainShader = m_state.ds.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.ds.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.ds.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShader(
          ID3D11GeometryShader*             pShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11GeometryShader*>(pShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.gs.shader != shader) {
      m_state.gs.shader = shader;

      BindShader(
        DxbcProgramType::GeometryShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::GeometryShader,
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void D3D11DeviceContext::GSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {  
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::GeometryShader,
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::GeometryShader,
      m_state.gs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
      DxbcProgramType::GeometryShader,
      m_state.gs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShader(
          ID3D11GeometryShader**            ppGeometryShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppGeometryShader != nullptr)
      *ppGeometryShader = m_state.gs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void D3D11DeviceContext::GSGetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer**                    ppConstantBuffers, 
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.gs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.gs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShader(
          ID3D11PixelShader*                pPixelShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11PixelShader*>(pPixelShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.ps.shader != shader) {
      m_state.ps.shader = shader;

      BindShader(
        DxbcProgramType::PixelShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::PixelShader,
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void D3D11DeviceContext::PSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::PixelShader,
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::PixelShader,
      m_state.ps.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
      DxbcProgramType::PixelShader,
      m_state.ps.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShader(
          ID3D11PixelShader**               ppPixelShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppPixelShader != nullptr)
      *ppPixelShader = m_state.ps.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void D3D11DeviceContext::PSGetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers, 
          ID3D11Buffer**                    ppConstantBuffers,
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.ps.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.ps.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShader(
          ID3D11ComputeShader*              pComputeShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    auto shader = static_cast<D3D11ComputeShader*>(pComputeShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11: Class instances not supported");
    
    if (m_state.cs.shader != shader) {
      m_state.cs.shader = shader;

      BindShader(
        DxbcProgramType::ComputeShader,
        GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::ComputeShader,
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers(
      DxbcProgramType::ComputeShader,
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources(
      DxbcProgramType::ComputeShader,
      m_state.cs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers(
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
    D3D10DeviceLock lock = LockContext();
    
    SetUnorderedAccessViews(
      DxbcProgramType::ComputeShader,
      m_state.cs.unorderedAccessViews,
      StartSlot, NumUAVs,
      ppUnorderedAccessViews,
      pUAVInitialCounts);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShader(
          ID3D11ComputeShader**             ppComputeShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppComputeShader != nullptr)
      *ppComputeShader = m_state.cs.shader.ref();
    
    if (pNumClassInstances != nullptr)
      *pNumClassInstances = 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      nullptr, nullptr);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers, 
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    GetConstantBuffers(
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers,
      pFirstConstant,
      pNumConstants);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumViews; i++)
      ppShaderResourceViews[i] = m_state.cs.shaderResources.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = m_state.cs.samplers.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumUAVs; i++)
      ppUnorderedAccessViews[i] = m_state.cs.unorderedAccessViews.at(StartSlot + i).ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    D3D10DeviceLock lock = LockContext();
    
    SetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
    BindFramebuffer(false);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    D3D10DeviceLock lock = LockContext();
    
    bool isUavRendering = false;
    
    if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
      SetRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView);
    
    if (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS) {
      // Check whether there actually are any UAVs bound
      for (uint32_t i = 0; i < NumUAVs && !isUavRendering; i++)
        isUavRendering = ppUnorderedAccessViews[i] != nullptr;

      // UAVs are made available to all shader stages in
      // the graphics pipeline even though this code may
      // suggest that they are limited to the pixel shader.
      SetUnorderedAccessViews(
        DxbcProgramType::PixelShader,
        m_state.ps.unorderedAccessViews,
        UAVStartSlot, NumUAVs,
        ppUnorderedAccessViews,
        pUAVInitialCounts);
    }

    BindFramebuffer(isUavRendering);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetBlendState(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetDepthStencilState(
          ID3D11DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    D3D10DeviceLock lock = LockContext();
    
    auto depthStencilState = static_cast<D3D11DepthStencilState*>(pDepthStencilState);
    
    if (m_state.om.dsState != depthStencilState) {
      m_state.om.dsState = depthStencilState;
      ApplyDepthStencilState();
    }
    
    if (m_state.om.stencilRef != StencilRef) {
      m_state.om.stencilRef = StencilRef;
      ApplyStencilRef();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppRenderTargetViews != nullptr) {
      for (UINT i = 0; i < NumViews; i++)
        ppRenderTargetViews[i] = m_state.om.renderTargetViews[i].ref();
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
    OMGetRenderTargets(NumRTVs, ppRenderTargetViews, ppDepthStencilView);
    
    D3D10DeviceLock lock = LockContext();
    
    if (ppUnorderedAccessViews != nullptr) {
      for (UINT i = 0; i < NumUAVs; i++)
        ppUnorderedAccessViews[i] = m_state.ps.unorderedAccessViews[UAVStartSlot + i].ref();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMGetBlendState(
          ID3D11BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    D3D10DeviceLock lock = LockContext();
    
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
    D3D10DeviceLock lock = LockContext();
    
    if (ppDepthStencilState != nullptr)
      *ppDepthStencilState = m_state.om.dsState.ref();
    
    if (pStencilRef != nullptr)
      *pStencilRef = m_state.om.stencilRef;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    D3D10DeviceLock lock = LockContext();
    
    auto rasterizerState = static_cast<D3D11RasterizerState*>(pRasterizerState);
    
    if (m_state.rs.state != rasterizerState) {
      m_state.rs.state = rasterizerState;
      
      // In D3D11, the rasterizer state defines whether the
      // scissor test is enabled, so we have to update the
      // scissor rectangles as well.
      ApplyRasterizerState();
      ApplyViewportState();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetViewports(
          UINT                              NumViewports,
    const D3D11_VIEWPORT*                   pViewports) {
    D3D10DeviceLock lock = LockContext();
    
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetScissorRects(
          UINT                              NumRects,
    const D3D11_RECT*                       pRects) {
    D3D10DeviceLock lock = LockContext();
    
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppRasterizerState != nullptr)
      *ppRasterizerState = m_state.rs.state.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    D3D10DeviceLock lock = LockContext();
    
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
    D3D10DeviceLock lock = LockContext();
    
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SOGetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumBuffers; i++)
      ppSOTargets[i] = m_state.so.targets[i].buffer.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SOGetTargetsWithOffsets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets,
          UINT*                             pOffsets) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      if (ppSOTargets != nullptr)
        ppSOTargets[i] = m_state.so.targets[i].buffer.ref();

      if (pOffsets != nullptr)
        pOffsets[i] = m_state.so.targets[i].offset;
    }
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::TransitionSurfaceLayout(
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
  
  
  void D3D11DeviceContext::ApplyInputLayout() {
    if (m_state.ia.inputLayout != nullptr) {
      EmitCs([cInputLayout = m_state.ia.inputLayout] (DxvkContext* ctx) {
        cInputLayout->BindToContext(ctx);
      });
    } else {
      EmitCs([] (DxvkContext* ctx) {
        ctx->setInputLayout(0, nullptr, 0, nullptr);
      });
    }
  }
  
  
  void D3D11DeviceContext::ApplyPrimitiveTopology() {
    if (m_state.ia.primitiveTopology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
      return;
    
    const DxvkInputAssemblyState iaState =
      [Topology = m_state.ia.primitiveTopology] () -> DxvkInputAssemblyState {
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
  
  
  void D3D11DeviceContext::ApplyBlendState() {
    EmitCs([
      cBlendState = m_state.om.cbState != nullptr
        ? m_state.om.cbState
        : m_defaultBlendState,
      cSampleMask = m_state.om.sampleMask
    ] (DxvkContext* ctx) {
      cBlendState->BindToContext(ctx, cSampleMask);
    });
  }
  
  
  void D3D11DeviceContext::ApplyBlendFactor() {
    EmitCs([
      cBlendConstants = DxvkBlendConstants {
        m_state.om.blendFactor[0], m_state.om.blendFactor[1],
        m_state.om.blendFactor[2], m_state.om.blendFactor[3] }
    ] (DxvkContext* ctx) {
      ctx->setBlendConstants(cBlendConstants);
    });
  }
  
  
  void D3D11DeviceContext::ApplyDepthStencilState() {
    EmitCs([
      cDepthStencilState = m_state.om.dsState != nullptr
        ? m_state.om.dsState
        : m_defaultDepthStencilState
    ] (DxvkContext* ctx) {
      cDepthStencilState->BindToContext(ctx);
    });
  }
  
  
  void D3D11DeviceContext::ApplyStencilRef() {
    EmitCs([
      cStencilRef = m_state.om.stencilRef
    ] (DxvkContext* ctx) {
      ctx->setStencilReference(cStencilRef);
    });
  }
  
  
  void D3D11DeviceContext::ApplyRasterizerState() {
    EmitCs([
      cRasterizerState = m_state.rs.state != nullptr
        ? m_state.rs.state
        : m_defaultRasterizerState
    ] (DxvkContext* ctx) {
      cRasterizerState->BindToContext(ctx);
    });
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
      if (enableScissorTest && (i < m_state.rs.numScissors)) {
        D3D11_RECT sr = m_state.rs.scissors.at(i);
        
        VkOffset2D srPosA;
        srPosA.x = std::max<int32_t>(0, sr.left);
        srPosA.y = std::max<int32_t>(0, sr.top);
        
        VkOffset2D srPosB;
        srPosB.x = std::max<int32_t>(srPosA.x, sr.right);
        srPosB.y = std::max<int32_t>(srPosA.y, sr.bottom);
        
        VkExtent2D srSize;
        srSize.width  = uint32_t(srPosB.x - srPosA.x);
        srSize.height = uint32_t(srPosB.y - srPosA.y);
        
        scissors.at(i) = VkRect2D { srPosA, srSize };
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
  
  
  void D3D11DeviceContext::BindShader(
          DxbcProgramType       ShaderStage,
    const D3D11CommonShader*    pShaderModule) {
    // Bind the shader and the ICB at once
    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::ConstantBuffer,
      D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
    
    EmitCs([
      cSlotId = slotId,
      cStage  = GetShaderStage(ShaderStage),
      cSlice  = pShaderModule           != nullptr
             && pShaderModule->GetIcb() != nullptr
        ? DxvkBufferSlice(pShaderModule->GetIcb())
        : DxvkBufferSlice(),
      cShader = pShaderModule != nullptr
        ? pShaderModule->GetShader()
        : nullptr
    ] (DxvkContext* ctx) {
      ctx->bindShader        (cStage, cShader);
      ctx->bindResourceBuffer(cSlotId, cSlice);
    });
  }


  void D3D11DeviceContext::BindFramebuffer(BOOL Spill) {
    // NOTE According to the Microsoft docs, we are supposed to
    // unbind overlapping shader resource views. Since this comes
    // with a large performance penalty we'll ignore this until an
    // application actually relies on this behaviour.
    DxvkRenderTargets attachments;
    
    // D3D11 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      if (m_state.om.renderTargetViews.at(i) != nullptr) {
        attachments.color[i] = {
          m_state.om.renderTargetViews.at(i)->GetImageView(),
          m_state.om.renderTargetViews.at(i)->GetRenderLayout() };
      }
    }
    
    if (m_state.om.depthStencilView != nullptr) {
      attachments.depth = {
        m_state.om.depthStencilView->GetImageView(),
        m_state.om.depthStencilView->GetRenderLayout() };
    }
    
    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments),
      cSpill       = Spill
    ] (DxvkContext* ctx) {
      ctx->bindRenderTargets(cAttachments, cSpill);
    });
  }
  
  
  void D3D11DeviceContext::BindDrawBuffer(
          D3D11Buffer*                      pBuffer) {
    EmitCs([
      cBufferSlice = pBuffer != nullptr
        ? pBuffer->GetBufferSlice()
        : DxvkBufferSlice()
    ] (DxvkContext* ctx) {
      ctx->bindDrawBuffer(cBufferSlice);
    });
  }


  void D3D11DeviceContext::BindVertexBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Stride) {
    EmitCs([
      cSlotId       = Slot,
      cBufferSlice  = pBuffer != nullptr ? pBuffer->GetBufferSlice(Offset) : DxvkBufferSlice(),
      cStride       = pBuffer != nullptr ? Stride                          : 0
    ] (DxvkContext* ctx) {
      ctx->bindVertexBuffer(cSlotId, cBufferSlice, cStride);
    });
  }
  
  
  void D3D11DeviceContext::BindIndexBuffer(
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          DXGI_FORMAT                       Format) {
    // As in Vulkan, the index format can be either a 32-bit
    // or 16-bit unsigned integer, no other formats are allowed.
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    
    if (pBuffer != nullptr) {
      switch (Format) {
        case DXGI_FORMAT_R16_UINT: indexType = VK_INDEX_TYPE_UINT16; break;
        case DXGI_FORMAT_R32_UINT: indexType = VK_INDEX_TYPE_UINT32; break;
        default: Logger::err(str::format("D3D11: Invalid index format: ", Format));
      }
    }
    
    EmitCs([
      cBufferSlice  = pBuffer != nullptr ? pBuffer->GetBufferSlice(Offset) : DxvkBufferSlice(),
      cIndexType    = indexType
    ] (DxvkContext* ctx) {
      ctx->bindIndexBuffer(cBufferSlice, cIndexType);
    });
  }
  

  void D3D11DeviceContext::BindXfbBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset) {
    DxvkBufferSlice bufferSlice;
    DxvkBufferSlice counterSlice;
    
    if (pBuffer != nullptr) {
      bufferSlice  = pBuffer->GetBufferSlice();
      counterSlice = pBuffer->GetSOCounter();
    }

    EmitCs([
      cSlotId       = Slot,
      cOffset       = Offset,
      cBufferSlice  = bufferSlice,
      cCounterSlice = counterSlice
    ] (DxvkContext* ctx) {
      if (cCounterSlice.defined() && cOffset != ~0u) {
        ctx->updateBuffer(
          cCounterSlice.buffer(),
          cCounterSlice.offset(),
          sizeof(cOffset),
          &cOffset);
      }

      ctx->bindXfbBuffer(cSlotId, cBufferSlice, cCounterSlice);
    });
  }


  void D3D11DeviceContext::BindConstantBuffer(
          UINT                              Slot,
    const D3D11ConstantBufferBinding*       pBufferBinding) {
    EmitCs([
      cSlotId      = Slot,
      cBufferSlice = pBufferBinding->buffer != nullptr
        ? pBufferBinding->buffer->GetBufferSlice(
            pBufferBinding->constantOffset * 16,
            pBufferBinding->constantCount  * 16)
        : DxvkBufferSlice()
    ] (DxvkContext* ctx) {
      ctx->bindResourceBuffer(cSlotId, cBufferSlice);
    });
  }
  
  
  void D3D11DeviceContext::BindSampler(
          UINT                              Slot,
          D3D11SamplerState*                pSampler) {
    EmitCs([
      cSlotId   = Slot,
      cSampler  = pSampler != nullptr ? pSampler->GetDXVKSampler() : nullptr
    ] (DxvkContext* ctx) {
      ctx->bindResourceSampler(cSlotId, cSampler);
    });
  }
  
  
  void D3D11DeviceContext::BindShaderResource(
          UINT                              Slot,
          D3D11ShaderResourceView*          pResource) {
    EmitCs([
      cSlotId     = Slot,
      cImageView  = pResource != nullptr ? pResource->GetImageView()  : nullptr,
      cBufferView = pResource != nullptr ? pResource->GetBufferView() : nullptr
    ] (DxvkContext* ctx) {
      ctx->bindResourceView(cSlotId, cImageView, cBufferView);
    });
  }
  
  
  void D3D11DeviceContext::BindUnorderedAccessView(
          UINT                              UavSlot,
          D3D11UnorderedAccessView*         pUav,
          UINT                              CtrSlot,
          UINT                              Counter) {
    EmitCs([
      cUavSlotId    = UavSlot,
      cCtrSlotId    = CtrSlot,
      cImageView    = pUav != nullptr ? pUav->GetImageView()    : nullptr,
      cBufferView   = pUav != nullptr ? pUav->GetBufferView()   : nullptr,
      cCounterSlice = pUav != nullptr ? pUav->GetCounterSlice() : DxvkBufferSlice(),
      cCounterValue = Counter
    ] (DxvkContext* ctx) {
      if (cCounterSlice.defined() && cCounterValue != ~0u) {
        ctx->updateBuffer(
          cCounterSlice.buffer(),
          cCounterSlice.offset(),
          sizeof(uint32_t),
          &cCounterValue);
      }

      ctx->bindResourceView   (cUavSlotId, cImageView, cBufferView);
      ctx->bindResourceBuffer (cCtrSlotId, cCounterSlice);
    });
  }
  
  
  void D3D11DeviceContext::DiscardBuffer(
          D3D11Buffer*                      pBuffer) {
    EmitCs([cBuffer = pBuffer->GetBuffer()] (DxvkContext* ctx) {
      ctx->discardBuffer(cBuffer);
    });
  }


  void D3D11DeviceContext::DiscardTexture(
          D3D11CommonTexture*               pTexture) {
    EmitCs([cImage = pTexture->GetImage()] (DxvkContext* ctx) {
      VkImageSubresourceRange subresources = {
        cImage->formatInfo()->aspectMask,
        0, cImage->info().mipLevels,
        0, cImage->info().numLayers };
      ctx->discardImage(cImage, subresources);
    });
  }


  void D3D11DeviceContext::SetDrawBuffer(
          ID3D11Buffer*                     pBuffer) {
    auto buffer = static_cast<D3D11Buffer*>(pBuffer);

    if (m_state.id.argBuffer != buffer) {
      m_state.id.argBuffer = buffer;
      BindDrawBuffer(buffer);
    }
  }


  void D3D11DeviceContext::SetConstantBuffers(
          DxbcProgramType                   ShaderStage,
          D3D11ConstantBufferBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    const uint32_t slotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::ConstantBuffer,
      StartSlot);
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);
      
      UINT constantOffset = 0;
      UINT constantCount  = newBuffer != nullptr
        ? newBuffer->Desc()->ByteWidth / 16
        : 0;
      
      if (newBuffer != nullptr && pFirstConstant != nullptr && pNumConstants != nullptr) {
        constantOffset = pFirstConstant[i];
        constantCount  = pNumConstants [i];
      }
      
      if (Bindings[StartSlot + i].buffer         != newBuffer
       || Bindings[StartSlot + i].constantOffset != constantOffset
       || Bindings[StartSlot + i].constantCount  != constantCount) {
        Bindings[StartSlot + i].buffer         = newBuffer;
        Bindings[StartSlot + i].constantOffset = constantOffset;
        Bindings[StartSlot + i].constantCount  = constantCount;
        
        BindConstantBuffer(slotId + i, &Bindings[StartSlot + i]);
      }
    }
  }
  
  
  void D3D11DeviceContext::SetSamplers(
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
        BindSampler(slotId + i, sampler);
      }
    }
  }
  
  
  void D3D11DeviceContext::SetShaderResources(
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
        BindShaderResource(slotId + i, resView);
      }
    }
  }
  
  
  void D3D11DeviceContext::SetUnorderedAccessViews(
          DxbcProgramType                   ShaderStage,
          D3D11UnorderedAccessBindings&     Bindings,
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    const uint32_t uavSlotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::UnorderedAccessView,
      StartSlot);
    
    const uint32_t ctrSlotId = computeResourceSlotId(
      ShaderStage, DxbcBindingType::UavCounter,
      StartSlot);
    
    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);
      auto ctr = pUAVInitialCounts ? pUAVInitialCounts[i] : ~0u;
      
      if (Bindings[StartSlot + i] != uav || ctr != ~0u) {
        Bindings[StartSlot + i] = uav;
        
        BindUnorderedAccessView(
          uavSlotId + i, uav,
          ctrSlotId + i, ctr);
      }
    }
  }


  void D3D11DeviceContext::SetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    // Native D3D11 does not change the render targets if
    // the parameters passed to this method are invalid.
    if (!ValidateRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView))
      return;
    
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      m_state.om.renderTargetViews.at(i) = i < NumViews
        ? static_cast<D3D11RenderTargetView*>(ppRenderTargetViews[i])
        : nullptr;
    }
    
    m_state.om.depthStencilView = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
  }
  
  
  void D3D11DeviceContext::GetConstantBuffers(
    const D3D11ConstantBufferBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers, 
          UINT*                             pFirstConstant, 
          UINT*                             pNumConstants) {
    for (uint32_t i = 0; i < NumBuffers; i++) {
      if (ppConstantBuffers != nullptr)
        ppConstantBuffers[i] = Bindings[StartSlot + i].buffer.ref();
      
      if (pFirstConstant != nullptr)
        pFirstConstant[i] = Bindings[StartSlot + i].constantOffset;
      
      if (pNumConstants != nullptr)
        pNumConstants[i] = Bindings[StartSlot + i].constantCount;
    }
  }
  
  
  void D3D11DeviceContext::RestoreState() {
    BindFramebuffer(false);
    
    BindShader(DxbcProgramType::VertexShader,   GetCommonShader(m_state.vs.shader.ptr()));
    BindShader(DxbcProgramType::HullShader,     GetCommonShader(m_state.hs.shader.ptr()));
    BindShader(DxbcProgramType::DomainShader,   GetCommonShader(m_state.ds.shader.ptr()));
    BindShader(DxbcProgramType::GeometryShader, GetCommonShader(m_state.gs.shader.ptr()));
    BindShader(DxbcProgramType::PixelShader,    GetCommonShader(m_state.ps.shader.ptr()));
    BindShader(DxbcProgramType::ComputeShader,  GetCommonShader(m_state.cs.shader.ptr()));
    
    ApplyInputLayout();
    ApplyPrimitiveTopology();
    ApplyBlendState();
    ApplyBlendFactor();
    ApplyDepthStencilState();
    ApplyStencilRef();
    ApplyRasterizerState();
    ApplyViewportState();

    BindDrawBuffer(
      m_state.id.argBuffer.ptr());
    
    BindIndexBuffer(
      m_state.ia.indexBuffer.buffer.ptr(),
      m_state.ia.indexBuffer.offset,
      m_state.ia.indexBuffer.format);
    
    for (uint32_t i = 0; i < m_state.ia.vertexBuffers.size(); i++) {
      BindVertexBuffer(i,
        m_state.ia.vertexBuffers[i].buffer.ptr(),
        m_state.ia.vertexBuffers[i].offset,
        m_state.ia.vertexBuffers[i].stride);
    }

    for (uint32_t i = 0; i < m_state.so.targets.size(); i++)
      BindXfbBuffer(i, m_state.so.targets[i].buffer.ptr(), ~0u);
    
    RestoreConstantBuffers(DxbcProgramType::VertexShader,   m_state.vs.constantBuffers);
    RestoreConstantBuffers(DxbcProgramType::HullShader,     m_state.hs.constantBuffers);
    RestoreConstantBuffers(DxbcProgramType::DomainShader,   m_state.ds.constantBuffers);
    RestoreConstantBuffers(DxbcProgramType::GeometryShader, m_state.gs.constantBuffers);
    RestoreConstantBuffers(DxbcProgramType::PixelShader,    m_state.ps.constantBuffers);
    RestoreConstantBuffers(DxbcProgramType::ComputeShader,  m_state.cs.constantBuffers);
    
    RestoreSamplers(DxbcProgramType::VertexShader,   m_state.vs.samplers);
    RestoreSamplers(DxbcProgramType::HullShader,     m_state.hs.samplers);
    RestoreSamplers(DxbcProgramType::DomainShader,   m_state.ds.samplers);
    RestoreSamplers(DxbcProgramType::GeometryShader, m_state.gs.samplers);
    RestoreSamplers(DxbcProgramType::PixelShader,    m_state.ps.samplers);
    RestoreSamplers(DxbcProgramType::ComputeShader,  m_state.cs.samplers);
    
    RestoreShaderResources(DxbcProgramType::VertexShader,   m_state.vs.shaderResources);
    RestoreShaderResources(DxbcProgramType::HullShader,     m_state.hs.shaderResources);
    RestoreShaderResources(DxbcProgramType::DomainShader,   m_state.ds.shaderResources);
    RestoreShaderResources(DxbcProgramType::GeometryShader, m_state.gs.shaderResources);
    RestoreShaderResources(DxbcProgramType::PixelShader,    m_state.ps.shaderResources);
    RestoreShaderResources(DxbcProgramType::ComputeShader,  m_state.cs.shaderResources);
    
    RestoreUnorderedAccessViews(DxbcProgramType::PixelShader,   m_state.ps.unorderedAccessViews);
    RestoreUnorderedAccessViews(DxbcProgramType::ComputeShader, m_state.cs.unorderedAccessViews);
  }
  
  
  void D3D11DeviceContext::RestoreConstantBuffers(
          DxbcProgramType                   Stage,
          D3D11ConstantBufferBindings&      Bindings) {
    const uint32_t slotId = computeResourceSlotId(
      Stage, DxbcBindingType::ConstantBuffer, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++)
      BindConstantBuffer(slotId + i, &Bindings[i]);
  }
  
  
  void D3D11DeviceContext::RestoreSamplers(
          DxbcProgramType                   Stage,
          D3D11SamplerBindings&             Bindings) {
    const uint32_t slotId = computeResourceSlotId(
      Stage, DxbcBindingType::ImageSampler, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++)
      BindSampler(slotId + i, Bindings[i].ptr());
  }
  
  
  void D3D11DeviceContext::RestoreShaderResources(
          DxbcProgramType                   Stage,
          D3D11ShaderResourceBindings&      Bindings) {
    const uint32_t slotId = computeResourceSlotId(
      Stage, DxbcBindingType::ShaderResource, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++)
      BindShaderResource(slotId + i, Bindings[i].ptr());
  }
  
  
  void D3D11DeviceContext::RestoreUnorderedAccessViews(
          DxbcProgramType                   Stage,
          D3D11UnorderedAccessBindings&     Bindings) {
    const uint32_t uavSlotId = computeResourceSlotId(
      Stage, DxbcBindingType::UnorderedAccessView, 0);
    
    const uint32_t ctrSlotId = computeResourceSlotId(
      Stage, DxbcBindingType::UavCounter, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++) {
      BindUnorderedAccessView(
        uavSlotId + i,
        Bindings[i].ptr(),
        ctrSlotId + i, ~0u);
    }
  }
  
  
  bool D3D11DeviceContext::ValidateRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    Rc<DxvkImageView> refView;
    
    if (pDepthStencilView != nullptr) {
      refView = static_cast<D3D11DepthStencilView*>(
        pDepthStencilView)->GetImageView();
    }
    
    for (uint32_t i = 0; i < NumViews; i++) {
      if (ppRenderTargetViews[i] != nullptr) {
        auto curView = static_cast<D3D11RenderTargetView*>(
          ppRenderTargetViews[i])->GetImageView();
        
        if (refView != nullptr) {
          // Render target views must all have the same
          // size, sample count, layer count, and type
          if (curView->info().type      != refView->info().type
           || curView->info().numLayers != refView->info().numLayers)
            return false;
          
          if (curView->imageInfo().sampleCount
           != refView->imageInfo().sampleCount)
            return false;
        } else {
          // Set reference view. All remaining views
          // must be compatible to the reference view.
          refView = curView;
        }
      }
    }
    
    return true;
  }
  
  
  DxvkDataSlice D3D11DeviceContext::AllocUpdateBufferSlice(size_t Size) {
    constexpr size_t UpdateBufferSize = 16 * 1024 * 1024;
    
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
  
  
  DxvkCsChunkRef D3D11DeviceContext::AllocCsChunk() {
    return m_parent->AllocCsChunk(m_csFlags);
  }
  
}
