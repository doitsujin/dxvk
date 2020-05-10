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
    m_contextExt(this),
    m_annotation(this),
    m_multithread(this, false),
    m_device    (Device),
    m_csFlags   (CsFlags),
    m_csChunk   (AllocCsChunk()),
    m_cmdData   (nullptr) {

  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::QueryInterface(REFIID riid, void** ppvObject) {
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
    
    if (riid == __uuidof(ID3D11VkExtContext)) {
      *ppvObject = ref(&m_contextExt);
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

    if (resType == D3D11_RESOURCE_DIMENSION_BUFFER) {
      DiscardBuffer(pResource);
    } else {
      auto texture = GetCommonTexture(pResource);

      for (uint32_t i = 0; i < texture->CountSubresources(); i++)
        DiscardTexture(pResource, i);
    }
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView(ID3D11View* pResourceView) {
    DiscardView1(pResourceView, nullptr, 0);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView1(
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
      m_state.vs.shaderResources.views[i] = nullptr;
      m_state.hs.shaderResources.views[i] = nullptr;
      m_state.ds.shaderResources.views[i] = nullptr;
      m_state.gs.shaderResources.views[i] = nullptr;
      m_state.ps.shaderResources.views[i] = nullptr;
      m_state.cs.shaderResources.views[i] = nullptr;
    }

    m_state.vs.shaderResources.hazardous.clear();
    m_state.hs.shaderResources.hazardous.clear();
    m_state.ds.shaderResources.hazardous.clear();
    m_state.gs.shaderResources.hazardous.clear();
    m_state.ps.shaderResources.hazardous.clear();
    m_state.cs.shaderResources.hazardous.clear();
    
    // Default UAVs
    for (uint32_t i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++) {
      m_state.ps.unorderedAccessViews[i] = nullptr;
      m_state.cs.unorderedAccessViews[i] = nullptr;
    }

    m_state.cs.uavMask.clear();

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
      m_state.om.blendFactor[i] = 1.0f;
    
    m_state.om.sampleMask = D3D11_DEFAULT_SAMPLE_MASK;
    m_state.om.stencilRef = D3D11_DEFAULT_STENCIL_REFERENCE;

    m_state.om.maxRtv = 0;
    m_state.om.maxUav = 0;
    
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
    ResetState();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetPredication(
          ID3D11Predicate*                  pPredicate,
          BOOL                              PredicateValue) {
    D3D10DeviceLock lock = LockContext();

    auto predicate = D3D11Query::FromPredicate(pPredicate);
    m_state.pr.predicateObject = predicate;
    m_state.pr.predicateValue  = PredicateValue;

    static bool s_errorShown = false;

    if (pPredicate && !std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetPredication: Stub");

    // TODO: Figure out why this breaks Watch Dogs and crashes War Thunder
    // if (!m_device->features().extConditionalRendering.conditionalRendering)
    //   return;

    // EmitCs([
    //   cPredicate = Com<D3D11Query, false>(predicate),
    //   cValue     = PredicateValue
    // ] (DxvkContext* ctx) {
    //   DxvkBufferSlice predSlice;

    //   if (cPredicate != nullptr)
    //     predSlice = cPredicate->GetPredicate(ctx);
      
    //   ctx->setPredicate(predSlice,
    //     cValue ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0);
    // });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GetPredication(
          ID3D11Predicate**                 ppPredicate,
          BOOL*                             pPredicateValue) {
    D3D10DeviceLock lock = LockContext();

    if (ppPredicate)
      *ppPredicate = D3D11Query::AsPredicate(m_state.pr.predicateObject.ref());
    
    if (pPredicateValue)
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
      
      if (DstSubresource >= dstTextureInfo->CountSubresources()
       || SrcSubresource >= srcTextureInfo->CountSubresources())
        return;
      
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

      if (dstTextureInfo->CanUpdateMappedBufferEarly())
        UpdateMappedBuffer(dstTextureInfo, dstSubresource);
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
      auto dstTexture = GetCommonTexture(pDstResource);
      auto srcTexture = GetCommonTexture(pSrcResource);

      const Rc<DxvkImage> dstImage = dstTexture->GetImage();
      const Rc<DxvkImage> srcImage = srcTexture->GetImage();

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

        if (dstTexture->CanUpdateMappedBufferEarly()) {
          for (uint32_t j = 0; j < dstImage->info().numLayers; j++)
            UpdateMappedBuffer(dstTexture, { dstLayers.aspectMask, i, j });
        }
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

    auto counterSlice = uav->GetCounterSlice();
    if (!counterSlice.defined())
      return;

    EmitCs([
      cDstSlice = buf->GetBufferSlice(DstAlignedByteOffset),
      cSrcSlice = std::move(counterSlice)
    ] (DxvkContext* ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset(),
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        sizeof(uint32_t));
    });
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::CopyTiles(
          ID3D11Resource*                   pTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          ID3D11Buffer*                     pBuffer,
          UINT64                            BufferStartOffsetInBytes,
          UINT                              Flags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::CopyTiles: Not implemented");
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::CopyTileMappings(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestRegionStartCoordinate,
          ID3D11Resource*                   pSourceTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pSourceRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          UINT                              Flags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::CopyTileMappings: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::ResizeTilePool(
          ID3D11Buffer*                     pTilePool,
          UINT64                            NewSizeInBytes) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::ResizeTilePool: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::TiledResourceBarrier(
          ID3D11DeviceChild*                pTiledResourceOrViewAccessBeforeBarrier,
          ID3D11DeviceChild*                pTiledResourceOrViewAccessAfterBarrier) {
    
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::ClearRenderTargetView(
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
          VK_IMAGE_ASPECT_COLOR_BIT,
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::ClearDepthStencilView(
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
  

  void STDMETHODCALLTYPE D3D11DeviceContext::ClearView(
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
  

  void STDMETHODCALLTYPE D3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) {
    D3D10DeviceLock lock = LockContext();

    auto view = static_cast<D3D11ShaderResourceView*>(pShaderResourceView);

    if (!view || view->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER)
      return;

    D3D11_COMMON_RESOURCE_DESC resourceDesc = view->GetResourceDesc();

    if (!(resourceDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS))
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
    
    // Filter out invalid copy flags
    CopyFlags &= D3D11_COPY_NO_OVERWRITE | D3D11_COPY_DISCARD;

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
      
      if (!size || offset + size > bufferSlice.length())
        return;

      bool useMap = (bufferSlice.buffer()->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                 && (size == bufferSlice.length() || CopyFlags);
      
      if (useMap) {
        D3D11_MAP mapType = (CopyFlags & D3D11_COPY_NO_OVERWRITE)
          ? D3D11_MAP_WRITE_NO_OVERWRITE
          : D3D11_MAP_WRITE_DISCARD;

        D3D11_MAPPED_SUBRESOURCE mappedSr;
        if (likely(useMap = SUCCEEDED(Map(pDstResource, 0, mapType, 0, &mappedSr)))) {
          std::memcpy(reinterpret_cast<char*>(mappedSr.pData) + offset, pSrcData, size);
          Unmap(pDstResource, 0);
        }
      }

      if (!useMap) {
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
      
      if (DstSubresource >= textureInfo->CountSubresources())
        return;
      
      VkFormat packedFormat = m_parent->LookupPackedFormat(
        textureInfo->Desc()->Format,
        textureInfo->GetFormatMode()).Format;

      auto formatInfo = imageFormatInfo(packedFormat);
      auto subresource = textureInfo->GetSubresourceFromIndex(
          formatInfo->aspectMask, DstSubresource);
      
      VkExtent3D mipExtent = textureInfo->GetImage()->mipLevelExtent(subresource.mipLevel);

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
      
      const VkImageSubresourceLayers layers = {
        subresource.aspectMask,
        subresource.mipLevel,
        subresource.arrayLayer, 1 };
      
      if (!util::isBlockAligned(offset, formatInfo->blockSize)
       || !util::isBlockAligned(offset, extent, formatInfo->blockSize, mipExtent))
        return;

      const VkExtent3D regionExtent = util::computeBlockCount(extent, formatInfo->blockSize);
      
      const VkDeviceSize bytesPerRow   = regionExtent.width  * formatInfo->elementSize;
      const VkDeviceSize bytesPerLayer = regionExtent.height * bytesPerRow;
      const VkDeviceSize bytesTotal    = regionExtent.depth  * bytesPerLayer;
      
      DxvkDataSlice imageDataBuffer = AllocUpdateBufferSlice(bytesTotal);
      
      util::packImageData(imageDataBuffer.ptr(), pSrcData,
        regionExtent, formatInfo->elementSize,
        SrcRowPitch, SrcDepthPitch);
      
      EmitCs([
        cDstImage         = textureInfo->GetImage(),
        cDstLayers        = layers,
        cDstOffset        = offset,
        cDstExtent        = extent,
        cSrcData          = std::move(imageDataBuffer),
        cSrcBytesPerRow   = bytesPerRow,
        cSrcBytesPerLayer = bytesPerLayer,
        cPackedFormat     = packedFormat
      ] (DxvkContext* ctx) {
        if (cDstLayers.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
          ctx->updateImage(cDstImage, cDstLayers,
            cDstOffset, cDstExtent, cSrcData.ptr(),
            cSrcBytesPerRow, cSrcBytesPerLayer);
        } else {
          ctx->updateDepthStencilImage(cDstImage, cDstLayers,
            VkOffset2D { cDstOffset.x,     cDstOffset.y      },
            VkExtent2D { cDstExtent.width, cDstExtent.height },
            cSrcData.ptr(), cSrcBytesPerRow, cSrcBytesPerLayer,
            cPackedFormat);
        }
      });

      if (textureInfo->CanUpdateMappedBufferEarly())
        UpdateMappedBuffer(textureInfo, subresource);
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::UpdateTileMappings(
          ID3D11Resource*                   pTiledResource,
          UINT                              NumTiledResourceRegions,
    const D3D11_TILED_RESOURCE_COORDINATE*  pTiledResourceRegionStartCoordinates,
    const D3D11_TILE_REGION_SIZE*           pTiledResourceRegionSizes,
          ID3D11Buffer*                     pTilePool,
          UINT                              NumRanges,
    const UINT*                             pRangeFlags,
    const UINT*                             pTilePoolStartOffsets,
    const UINT*                             pRangeTileCounts,
          UINT                              Flags) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::UpdateTileMappings: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::UpdateTiles(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pDestTileRegionSize,
    const void*                             pSourceTileData,
          UINT                              Flags) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::UpdateTiles: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetResourceMinLOD(
          ID3D11Resource*                   pResource,
          FLOAT                             MinLOD) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetResourceMinLOD: Not implemented");
  }
  
  
  FLOAT STDMETHODCALLTYPE D3D11DeviceContext::GetResourceMinLOD(ID3D11Resource* pResource) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
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
        VkImageResolve region;
        region.srcSubresource = cSrcSubres;
        region.srcOffset      = VkOffset3D { 0, 0, 0 };
        region.dstSubresource = cDstSubres;
        region.dstOffset      = VkOffset3D { 0, 0, 0 };
        region.extent         = cDstImage->mipLevelExtent(cDstSubres.mipLevel);

        ctx->resolveImage(cDstImage, cSrcImage, region, cFormat);
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
        vtxBuf.offset());
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
    SetDrawBuffers(pBufferForArgs, nullptr);
    
    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    constexpr VkDeviceSize stride = sizeof(VkDrawIndexedIndirectCommand);
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);

    bool useMultiDraw = cmdData && cmdData->type == D3D11CmdType::DrawIndirectIndexed
      && cmdData->offset + cmdData->count * stride == AlignedByteOffsetForArgs;
    
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
    SetDrawBuffers(pBufferForArgs, nullptr);

    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    constexpr VkDeviceSize stride = sizeof(VkDrawIndirectCommand);
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);

    bool useMultiDraw = cmdData && cmdData->type == D3D11CmdType::DrawIndirect
      && cmdData->offset + cmdData->count * stride == AlignedByteOffsetForArgs;
    
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
    SetDrawBuffers(pBufferForArgs, nullptr);
    
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
      bool needsUpdate = m_state.ia.vertexBuffers[StartSlot + i].buffer != newBuffer;

      if (needsUpdate)
        m_state.ia.vertexBuffers[StartSlot + i].buffer = newBuffer;

      needsUpdate |= m_state.ia.vertexBuffers[StartSlot + i].offset != pOffsets[i]
                  || m_state.ia.vertexBuffers[StartSlot + i].stride != pStrides[i];

      if (needsUpdate) {
        m_state.ia.vertexBuffers[StartSlot + i].offset = pOffsets[i];
        m_state.ia.vertexBuffers[StartSlot + i].stride = pStrides[i];

        BindVertexBuffer(StartSlot + i, newBuffer, pOffsets[i], pStrides[i]);
      }
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    D3D10DeviceLock lock = LockContext();
    
    auto newBuffer = static_cast<D3D11Buffer*>(pIndexBuffer);
    bool needsUpdate = m_state.ia.indexBuffer.buffer != newBuffer;

    if (needsUpdate)
      m_state.ia.indexBuffer.buffer = newBuffer;

    needsUpdate |= m_state.ia.indexBuffer.offset != Offset
                || m_state.ia.indexBuffer.format != Format;

    if (needsUpdate) {
      m_state.ia.indexBuffer.offset = Offset;
      m_state.ia.indexBuffer.format = Format;

      BindIndexBuffer(newBuffer, Offset, Format);
    }
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

      BindShader<DxbcProgramType::VertexShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::VertexShader>(
      m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::VertexShader>(
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
    
    SetShaderResources<DxbcProgramType::VertexShader>(
      m_state.vs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers<DxbcProgramType::VertexShader>(
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
      ppShaderResourceViews[i] = m_state.vs.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.vs.samplers[StartSlot + i]);
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

      BindShader<DxbcProgramType::HullShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources<DxbcProgramType::HullShader>(
      m_state.hs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::HullShader>(
      m_state.hs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers1(
          UINT                              StartSlot,
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::HullShader>(
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
    
    SetSamplers<DxbcProgramType::HullShader>(
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
      ppShaderResourceViews[i] = m_state.hs.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::HSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.hs.samplers[StartSlot + i]);
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

      BindShader<DxbcProgramType::DomainShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    D3D10DeviceLock lock = LockContext();
    
    SetShaderResources<DxbcProgramType::DomainShader>(
      m_state.ds.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::DomainShader>(
      m_state.ds.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::DomainShader>(
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
    
    SetSamplers<DxbcProgramType::DomainShader>(
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
      ppShaderResourceViews[i] = m_state.ds.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.ds.samplers[StartSlot + i]);
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

      BindShader<DxbcProgramType::GeometryShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::GeometryShader>(
      m_state.gs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void D3D11DeviceContext::GSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {  
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::GeometryShader>(
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
    
    SetShaderResources<DxbcProgramType::GeometryShader>(
      m_state.gs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers<DxbcProgramType::GeometryShader>(
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
      ppShaderResourceViews[i] = m_state.gs.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.gs.samplers[StartSlot + i]);
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

      BindShader<DxbcProgramType::PixelShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::PixelShader>(
      m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void D3D11DeviceContext::PSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::PixelShader>(
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
    
    SetShaderResources<DxbcProgramType::PixelShader>(
      m_state.ps.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers<DxbcProgramType::PixelShader>(
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
      ppShaderResourceViews[i] = m_state.ps.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.ps.samplers[StartSlot + i]);
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

      BindShader<DxbcProgramType::ComputeShader>(GetCommonShader(shader));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers<DxbcProgramType::ComputeShader>(
      m_state.cs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers1(
          UINT                              StartSlot, 
          UINT                              NumBuffers, 
          ID3D11Buffer* const*              ppConstantBuffers, 
    const UINT*                             pFirstConstant, 
    const UINT*                             pNumConstants) {
    D3D10DeviceLock lock = LockContext();
    
    SetConstantBuffers1<DxbcProgramType::ComputeShader>(
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
    
    SetShaderResources<DxbcProgramType::ComputeShader>(
      m_state.cs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    SetSamplers<DxbcProgramType::ComputeShader>(
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

    if (TestRtvUavHazards(0, nullptr, NumUAVs, ppUnorderedAccessViews))
      return;
    
    // Unbind previously bound conflicting UAVs
    uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::ComputeShader, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::ComputeShader, 0);

    int32_t uavId = m_state.cs.uavMask.findNext(0);

    while (uavId >= 0) {
      if (uint32_t(uavId) < StartSlot || uint32_t(uavId) >= StartSlot + NumUAVs) {
        for (uint32_t i = 0; i < NumUAVs; i++) {
          auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);

          if (CheckViewOverlap(uav, m_state.cs.unorderedAccessViews[uavId].ptr())) {
            m_state.cs.unorderedAccessViews[uavId] = nullptr;
            m_state.cs.uavMask.clr(uavId);

            BindUnorderedAccessView(
              uavSlotId + uavId, nullptr,
              ctrSlotId + uavId, ~0u);
          }
        }

        uavId = m_state.cs.uavMask.findNext(uavId + 1);
      } else {
        uavId = m_state.cs.uavMask.findNext(StartSlot + NumUAVs);
      }
    }

    // Actually bind the given UAVs
    for (uint32_t i = 0; i < NumUAVs; i++) {
      auto uav = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[i]);
      auto ctr = pUAVInitialCounts ? pUAVInitialCounts[i] : ~0u;

      if (m_state.cs.unorderedAccessViews[StartSlot + i] != uav || ctr != ~0u) {
        m_state.cs.unorderedAccessViews[StartSlot + i] = uav;
        m_state.cs.uavMask.set(StartSlot + i, uav != nullptr);

        BindUnorderedAccessView(
          uavSlotId + StartSlot + i, uav,
          ctrSlotId + StartSlot + i, ctr);
        
        ResolveCsSrvHazards(uav);
      }
    }
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
      ppShaderResourceViews[i] = m_state.cs.shaderResources.views[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumSamplers; i++)
      ppSamplers[i] = ref(m_state.cs.samplers[StartSlot + i]);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CSGetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    D3D10DeviceLock lock = LockContext();
    
    for (uint32_t i = 0; i < NumUAVs; i++)
      ppUnorderedAccessViews[i] = m_state.cs.unorderedAccessViews[StartSlot + i].ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    OMSetRenderTargetsAndUnorderedAccessViews(
      NumViews, ppRenderTargetViews, pDepthStencilView,
      NumViews, 0, nullptr, nullptr);
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

    if (TestRtvUavHazards(NumRTVs, ppRenderTargetViews, NumUAVs, ppUnorderedAccessViews))
      return;
    
    bool needsUpdate = false;

    if (likely(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)) {
      // Native D3D11 does not change the render targets if
      // the parameters passed to this method are invalid.
      if (!ValidateRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView))
        return;
      
      for (uint32_t i = 0; i < m_state.om.renderTargetViews.size(); i++) {
        auto rtv = i < NumRTVs
          ? static_cast<D3D11RenderTargetView*>(ppRenderTargetViews[i])
          : nullptr;
        
        if (m_state.om.renderTargetViews[i] != rtv) {
          m_state.om.renderTargetViews[i] = rtv;
          needsUpdate = true;
          ResolveOmSrvHazards(rtv);

          if (NumUAVs == D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
            ResolveOmUavHazards(rtv);
        }
      }

      auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);

      if (m_state.om.depthStencilView != dsv) {
        m_state.om.depthStencilView = dsv;
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

          if (m_state.ps.unorderedAccessViews[i] != uav || ctr != ~0u) {
            m_state.ps.unorderedAccessViews[i] = uav;

            BindUnorderedAccessView(
              uavSlotId + i, uav,
              ctrSlotId + i, ctr);
            
            ResolveOmSrvHazards(uav);

            if (NumRTVs == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
              needsUpdate |= ResolveOmRtvHazards(uav);
          }
        }
      }
    }

    if (needsUpdate)
      BindFramebuffer();
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
      *ppBlendState = ref(m_state.om.cbState);
    
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
      *ppDepthStencilState = ref(m_state.om.dsState);
    
    if (pStencilRef != nullptr)
      *pStencilRef = m_state.om.stencilRef;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    D3D10DeviceLock lock = LockContext();
    
    auto rasterizerState = static_cast<D3D11RasterizerState*>(pRasterizerState);
    
    bool currScissorEnable = m_state.rs.state != nullptr
      ? m_state.rs.state->Desc()->ScissorEnable
      : false;
    
    bool nextScissorEnable = rasterizerState != nullptr
      ? rasterizerState->Desc()->ScissorEnable
      : false;

    if (m_state.rs.state != rasterizerState) {
      m_state.rs.state = rasterizerState;

      // In D3D11, the rasterizer state defines whether the
      // scissor test is enabled, so we have to update the
      // scissor rectangles as well.
      ApplyRasterizerState();

      if (currScissorEnable != nextScissorEnable)
        ApplyViewportState();
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetViewports(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSSetScissorRects(
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
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    D3D10DeviceLock lock = LockContext();
    
    if (ppRasterizerState != nullptr)
      *ppRasterizerState = ref(m_state.rs.state);
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    D3D10DeviceLock lock = LockContext();
    
    if (pViewports != nullptr) {
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
    } else {
      *pNumViewports = m_state.rs.numViewports;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::RSGetScissorRects(
          UINT*                             pNumRects,
          D3D11_RECT*                       pRects) {
    D3D10DeviceLock lock = LockContext();
    
    if (pRects != nullptr) {
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
    } else {
      *pNumRects = m_state.rs.numScissors;
    }
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


  BOOL STDMETHODCALLTYPE D3D11DeviceContext::IsAnnotationEnabled() {
    // Not implemented in the backend
    return FALSE;
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::SetMarkerInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::BeginEventInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::EndEvent() {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::GetHardwareProtectionState(
          BOOL*                             pHwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::GetHardwareProtectionState: Not implemented");
    
    if (pHwProtectionEnable)
      *pHwProtectionEnable = FALSE;
  }

  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetHardwareProtectionState(
          BOOL                              HwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetHardwareProtectionState: Not implemented");
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
  
  
  void D3D11DeviceContext::ApplyPrimitiveTopology() {
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
  
  
  void D3D11DeviceContext::ApplyBlendState() {
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
  
  
  void D3D11DeviceContext::ApplyStencilRef() {
    EmitCs([
      cStencilRef = m_state.om.stencilRef
    ] (DxvkContext* ctx) {
      ctx->setStencilReference(cStencilRef);
    });
  }
  
  
  void D3D11DeviceContext::ApplyRasterizerState() {
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
  
  
  void D3D11DeviceContext::ApplyViewportState() {
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

  
  template<DxbcProgramType ShaderStage>
  void D3D11DeviceContext::BindShader(
    const D3D11CommonShader*    pShaderModule) {
    // Bind the shader and the ICB at once
    EmitCs([
      cSlice  = pShaderModule           != nullptr
             && pShaderModule->GetIcb() != nullptr
        ? DxvkBufferSlice(pShaderModule->GetIcb())
        : DxvkBufferSlice(),
      cShader = pShaderModule != nullptr
        ? pShaderModule->GetShader()
        : nullptr
    ] (DxvkContext* ctx) {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);

      uint32_t slotId = computeConstantBufferBinding(ShaderStage,
        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

      ctx->bindShader        (stage,  cShader);
      ctx->bindResourceBuffer(slotId, cSlice);
    });
  }


  void D3D11DeviceContext::BindFramebuffer() {
    DxvkRenderTargets attachments;
    
    // D3D11 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      if (m_state.om.renderTargetViews[i] != nullptr) {
        attachments.color[i] = {
          m_state.om.renderTargetViews[i]->GetImageView(),
          m_state.om.renderTargetViews[i]->GetRenderLayout() };
      }
    }
    
    if (m_state.om.depthStencilView != nullptr) {
      attachments.depth = {
        m_state.om.depthStencilView->GetImageView(),
        m_state.om.depthStencilView->GetRenderLayout() };
    }
    
    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments)
    ] (DxvkContext* ctx) {
      ctx->bindRenderTargets(cAttachments);
    });
  }
  
  
  void D3D11DeviceContext::BindDrawBuffers(
          D3D11Buffer*                     pBufferForArgs,
          D3D11Buffer*                     pBufferForCount) {
    EmitCs([
      cArgBuffer = pBufferForArgs  ? pBufferForArgs->GetBufferSlice()  : DxvkBufferSlice(),
      cCntBuffer = pBufferForCount ? pBufferForCount->GetBufferSlice() : DxvkBufferSlice()
    ] (DxvkContext* ctx) {
      ctx->bindDrawBuffers(cArgBuffer, cCntBuffer);
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
      cStride       = Stride
    ] (DxvkContext* ctx) {
      ctx->bindVertexBuffer(cSlotId, cBufferSlice, cStride);
    });
  }
  
  
  void D3D11DeviceContext::BindIndexBuffer(
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          DXGI_FORMAT                       Format) {
    VkIndexType indexType = Format == DXGI_FORMAT_R16_UINT
      ? VK_INDEX_TYPE_UINT16
      : VK_INDEX_TYPE_UINT32;
    
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
          D3D11Buffer*                      pBuffer) {
    EmitCs([
      cSlotId      = Slot,
      cBufferSlice = pBuffer ? pBuffer->GetBufferSlice() : DxvkBufferSlice()
    ] (DxvkContext* ctx) {
      ctx->bindResourceBuffer(cSlotId, cBufferSlice);
    });
  }
  
  
  void D3D11DeviceContext::BindConstantBuffer1(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Length) {
    EmitCs([
      cSlotId      = Slot,
      cBufferSlice = Length ? pBuffer->GetBufferSlice(16 * Offset, 16 * Length) : DxvkBufferSlice()
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
          ID3D11Resource*                   pResource) {
    auto buffer = static_cast<D3D11Buffer*>(pResource);

    if (buffer->GetMapMode() != D3D11_COMMON_BUFFER_MAP_MODE_NONE) {
      D3D11_MAPPED_SUBRESOURCE sr;

      Map(pResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
      Unmap(pResource, 0);
    }
  }


  void D3D11DeviceContext::DiscardTexture(
          ID3D11Resource*                   pResource,
          UINT                              Subresource) {
    auto texture = GetCommonTexture(pResource);

    if (texture->GetMapMode() != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      D3D11_MAPPED_SUBRESOURCE sr;

      Map(pResource, Subresource, D3D11_MAP_WRITE_DISCARD, 0, &sr);
      Unmap(pResource, Subresource);
    }
  }


  void D3D11DeviceContext::SetDrawBuffers(
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


  template<DxbcProgramType ShaderStage>
  void D3D11DeviceContext::SetConstantBuffers(
          D3D11ConstantBufferBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    uint32_t slotId = computeConstantBufferBinding(ShaderStage, StartSlot);
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);
      
      UINT constantBound = 0;
      
      if (likely(newBuffer != nullptr))
        constantBound = newBuffer->Desc()->ByteWidth / 16;
      
      if (Bindings[StartSlot + i].buffer         != newBuffer
       || Bindings[StartSlot + i].constantBound  != constantBound) {
        Bindings[StartSlot + i].buffer         = newBuffer;
        Bindings[StartSlot + i].constantOffset = 0;
        Bindings[StartSlot + i].constantCount  = constantBound;
        Bindings[StartSlot + i].constantBound  = constantBound;
        
        BindConstantBuffer(slotId + i, newBuffer);
      }
    }
  }
  
  
  template<DxbcProgramType ShaderStage>
  void D3D11DeviceContext::SetConstantBuffers1(
          D3D11ConstantBufferBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers,
    const UINT*                             pFirstConstant,
    const UINT*                             pNumConstants) {
    uint32_t slotId = computeConstantBufferBinding(ShaderStage, StartSlot);
    
    for (uint32_t i = 0; i < NumBuffers; i++) {
      auto newBuffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);
      
      UINT constantOffset;
      UINT constantCount;
      UINT constantBound;

      if (likely(newBuffer != nullptr)) {
        constantBound = newBuffer->Desc()->ByteWidth / 16;

        if (likely(pFirstConstant && pNumConstants)) {
          constantOffset  = pFirstConstant[i];
          constantCount   = pNumConstants [i];

          constantBound = (constantOffset + constantCount > constantBound)
            ? constantBound - std::min(constantOffset, constantBound)
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

      bool needsUpdate = Bindings[StartSlot + i].buffer != newBuffer;

      if (needsUpdate)
        Bindings[StartSlot + i].buffer = newBuffer;

      needsUpdate |= Bindings[StartSlot + i].constantOffset != constantOffset
                  || Bindings[StartSlot + i].constantCount  != constantCount;
      
      if (needsUpdate) {
        Bindings[StartSlot + i].constantOffset = constantOffset;
        Bindings[StartSlot + i].constantCount  = constantCount;
        Bindings[StartSlot + i].constantBound  = constantBound;
        
        BindConstantBuffer1(slotId + i, newBuffer, constantOffset, constantBound);
      }
    }
  }
  
  
  template<DxbcProgramType ShaderStage>
  void D3D11DeviceContext::SetSamplers(
          D3D11SamplerBindings&             Bindings,
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    uint32_t slotId = computeSamplerBinding(ShaderStage, StartSlot);
    
    for (uint32_t i = 0; i < NumSamplers; i++) {
      auto sampler = static_cast<D3D11SamplerState*>(ppSamplers[i]);
      
      if (Bindings[StartSlot + i] != sampler) {
        Bindings[StartSlot + i] = sampler;
        BindSampler(slotId + i, sampler);
      }
    }
  }
  
  
  template<DxbcProgramType ShaderStage>
  void D3D11DeviceContext::SetShaderResources(
          D3D11ShaderResourceBindings&      Bindings,
          UINT                              StartSlot,
          UINT                              NumResources,
          ID3D11ShaderResourceView* const*  ppResources) {
    uint32_t slotId = computeSrvBinding(ShaderStage, StartSlot);
    
    for (uint32_t i = 0; i < NumResources; i++) {
      auto resView = static_cast<D3D11ShaderResourceView*>(ppResources[i]);
      
      if (Bindings.views[StartSlot + i] != resView) {
        if (unlikely(resView && resView->TestHazards())) {
          if (TestSrvHazards<ShaderStage>(resView))
            resView = nullptr;

          // Only set if necessary, but don't reset it on every
          // bind as this would be more expensive than a few
          // redundant checks in OMSetRenderTargets and friends.
          Bindings.hazardous.set(StartSlot + i, resView);
        }

        Bindings.views[StartSlot + i] = resView;
        BindShaderResource(slotId + i, resView);
      }
    }
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
  
  
  void D3D11DeviceContext::ResetState() {
    EmitCs([] (DxvkContext* ctx) {
      // Reset render targets
      ctx->bindRenderTargets(DxvkRenderTargets());

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

      // Reset predication
      ctx->setPredicate(DxvkBufferSlice(), 0);

      // Unbind indirect draw buffer
      ctx->bindDrawBuffers(DxvkBufferSlice(), DxvkBufferSlice());

      // Unbind index and vertex buffers
      ctx->bindIndexBuffer(DxvkBufferSlice(), VK_INDEX_TYPE_UINT32);

      for (uint32_t i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
        ctx->bindVertexBuffer(i, DxvkBufferSlice(), 0);

      // Unbind transform feedback buffers
      for (uint32_t i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
        ctx->bindXfbBuffer(i, DxvkBufferSlice(), DxvkBufferSlice());

      // Unbind per-shader stage resources
      for (uint32_t i = 0; i < 6; i++) {
        auto programType = DxbcProgramType(i);
        ctx->bindShader(GetShaderStage(programType), nullptr);

        // Unbind constant buffers, including the shader's ICB
        auto cbSlotId = computeConstantBufferBinding(programType, 0);

        for (uint32_t j = 0; j <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; j++)
          ctx->bindResourceBuffer(cbSlotId + j, DxvkBufferSlice());

        // Unbind shader resource views
        auto srvSlotId = computeSrvBinding(programType, 0);

        for (uint32_t j = 0; j < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; j++)
          ctx->bindResourceView(srvSlotId + j, nullptr, nullptr);

        // Unbind texture samplers
        auto samplerSlotId = computeSamplerBinding(programType, 0);

        for (uint32_t j = 0; j < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; j++)
          ctx->bindResourceSampler(samplerSlotId + j, nullptr);

        // Unbind UAVs for supported stages
        if (programType == DxbcProgramType::PixelShader
         || programType == DxbcProgramType::ComputeShader) {
          auto uavSlotId = computeUavBinding(programType, 0);
          auto ctrSlotId = computeUavCounterBinding(programType, 0);

          for (uint32_t j = 0; j < D3D11_1_UAV_SLOT_COUNT; j++) {
            ctx->bindResourceView   (uavSlotId, nullptr, nullptr);
            ctx->bindResourceBuffer (ctrSlotId, DxvkBufferSlice());
          }
        }
      }
    });
  }


  void D3D11DeviceContext::RestoreState() {
    BindFramebuffer();
    
    BindShader<DxbcProgramType::VertexShader>   (GetCommonShader(m_state.vs.shader.ptr()));
    BindShader<DxbcProgramType::HullShader>     (GetCommonShader(m_state.hs.shader.ptr()));
    BindShader<DxbcProgramType::DomainShader>   (GetCommonShader(m_state.ds.shader.ptr()));
    BindShader<DxbcProgramType::GeometryShader> (GetCommonShader(m_state.gs.shader.ptr()));
    BindShader<DxbcProgramType::PixelShader>    (GetCommonShader(m_state.ps.shader.ptr()));
    BindShader<DxbcProgramType::ComputeShader>  (GetCommonShader(m_state.cs.shader.ptr()));
    
    ApplyInputLayout();
    ApplyPrimitiveTopology();
    ApplyBlendState();
    ApplyBlendFactor();
    ApplyDepthStencilState();
    ApplyStencilRef();
    ApplyRasterizerState();
    ApplyViewportState();

    BindDrawBuffers(
      m_state.id.argBuffer.ptr(),
      m_state.id.cntBuffer.ptr());
    
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
    
    RestoreConstantBuffers<DxbcProgramType::VertexShader>   (m_state.vs.constantBuffers);
    RestoreConstantBuffers<DxbcProgramType::HullShader>     (m_state.hs.constantBuffers);
    RestoreConstantBuffers<DxbcProgramType::DomainShader>   (m_state.ds.constantBuffers);
    RestoreConstantBuffers<DxbcProgramType::GeometryShader> (m_state.gs.constantBuffers);
    RestoreConstantBuffers<DxbcProgramType::PixelShader>    (m_state.ps.constantBuffers);
    RestoreConstantBuffers<DxbcProgramType::ComputeShader>  (m_state.cs.constantBuffers);
    
    RestoreSamplers<DxbcProgramType::VertexShader>  (m_state.vs.samplers);
    RestoreSamplers<DxbcProgramType::HullShader>    (m_state.hs.samplers);
    RestoreSamplers<DxbcProgramType::DomainShader>  (m_state.ds.samplers);
    RestoreSamplers<DxbcProgramType::GeometryShader>(m_state.gs.samplers);
    RestoreSamplers<DxbcProgramType::PixelShader>   (m_state.ps.samplers);
    RestoreSamplers<DxbcProgramType::ComputeShader> (m_state.cs.samplers);
    
    RestoreShaderResources<DxbcProgramType::VertexShader>   (m_state.vs.shaderResources);
    RestoreShaderResources<DxbcProgramType::HullShader>     (m_state.hs.shaderResources);
    RestoreShaderResources<DxbcProgramType::DomainShader>   (m_state.ds.shaderResources);
    RestoreShaderResources<DxbcProgramType::GeometryShader> (m_state.gs.shaderResources);
    RestoreShaderResources<DxbcProgramType::PixelShader>    (m_state.ps.shaderResources);
    RestoreShaderResources<DxbcProgramType::ComputeShader>  (m_state.cs.shaderResources);
    
    RestoreUnorderedAccessViews<DxbcProgramType::PixelShader>   (m_state.ps.unorderedAccessViews);
    RestoreUnorderedAccessViews<DxbcProgramType::ComputeShader> (m_state.cs.unorderedAccessViews);
  }
  
  
  template<DxbcProgramType Stage>
  void D3D11DeviceContext::RestoreConstantBuffers(
          D3D11ConstantBufferBindings&      Bindings) {
    uint32_t slotId = computeConstantBufferBinding(Stage, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++) {
      BindConstantBuffer1(slotId + i, Bindings[i].buffer.ptr(),
        Bindings[i].constantOffset, Bindings[i].constantBound);
    }
  }
  
  
  template<DxbcProgramType Stage>
  void D3D11DeviceContext::RestoreSamplers(
          D3D11SamplerBindings&             Bindings) {
    uint32_t slotId = computeSamplerBinding(Stage, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++)
      BindSampler(slotId + i, Bindings[i]);
  }
  
  
  template<DxbcProgramType Stage>
  void D3D11DeviceContext::RestoreShaderResources(
          D3D11ShaderResourceBindings&      Bindings) {
    uint32_t slotId = computeSrvBinding(Stage, 0);
    
    for (uint32_t i = 0; i < Bindings.views.size(); i++)
      BindShaderResource(slotId + i, Bindings.views[i].ptr());
  }
  
  
  template<DxbcProgramType Stage>
  void D3D11DeviceContext::RestoreUnorderedAccessViews(
          D3D11UnorderedAccessBindings&     Bindings) {
    uint32_t uavSlotId = computeUavBinding       (Stage, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(Stage, 0);
    
    for (uint32_t i = 0; i < Bindings.size(); i++) {
      BindUnorderedAccessView(
        uavSlotId + i,
        Bindings[i].ptr(),
        ctrSlotId + i, ~0u);
    }
  }


  void D3D11DeviceContext::UpdateMappedBuffer(
    const D3D11CommonTexture*               pTexture,
          VkImageSubresource                Subresource) {
    UINT SubresourceIndex = D3D11CalcSubresource(
      Subresource.mipLevel, Subresource.arrayLayer,
      pTexture->Desc()->MipLevels);

    Rc<DxvkImage>  mappedImage  = pTexture->GetImage();
    Rc<DxvkBuffer> mappedBuffer = pTexture->GetMappedBuffer(SubresourceIndex);

    VkFormat packedFormat = m_parent->LookupPackedFormat(
      pTexture->Desc()->Format, pTexture->GetFormatMode()).Format;
    
    VkExtent3D levelExtent = mappedImage->mipLevelExtent(Subresource.mipLevel);
    
    EmitCs([
      cImageBuffer  = std::move(mappedBuffer),
      cImage        = std::move(mappedImage),
      cSubresources = vk::makeSubresourceLayers(Subresource),
      cLevelExtent  = levelExtent,
      cPackedFormat = packedFormat
    ] (DxvkContext* ctx) {
      if (cSubresources.aspectMask != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        ctx->copyImageToBuffer(
          cImageBuffer, 0, VkExtent2D { 0u, 0u },
          cImage, cSubresources, VkOffset3D { 0, 0, 0 },
          cLevelExtent);
      } else {
        ctx->copyDepthStencilImageToPackedBuffer(
          cImageBuffer, 0, cImage, cSubresources,
          VkOffset2D { 0, 0 },
          VkExtent2D { cLevelExtent.width, cLevelExtent.height },
          cPackedFormat);
      }
    });
  }
  
  
  bool D3D11DeviceContext::TestRtvUavHazards(
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

  
  template<DxbcProgramType ShaderStage>
  bool D3D11DeviceContext::TestSrvHazards(
          D3D11ShaderResourceView*          pView) {
    bool hazard = false;

    if (ShaderStage == DxbcProgramType::ComputeShader) {
      int32_t uav = m_state.cs.uavMask.findNext(0);

      while (uav >= 0 && !hazard) {
        hazard = CheckViewOverlap(pView, m_state.cs.unorderedAccessViews[uav].ptr());
        uav = m_state.cs.uavMask.findNext(uav + 1);
      }
    } else {
      hazard = CheckViewOverlap(pView, m_state.om.depthStencilView.ptr());

      for (uint32_t i = 0; !hazard && i < m_state.om.maxRtv; i++)
        hazard = CheckViewOverlap(pView, m_state.om.renderTargetViews[i].ptr());

      for (uint32_t i = 0; !hazard && i < m_state.om.maxUav; i++)
        hazard = CheckViewOverlap(pView, m_state.ps.unorderedAccessViews[i].ptr());
    }

    return hazard;
  }


  template<DxbcProgramType ShaderStage, typename T>
  void D3D11DeviceContext::ResolveSrvHazards(
          T*                                pView,
          D3D11ShaderResourceBindings&      Bindings) {
    uint32_t slotId = computeSrvBinding(ShaderStage, 0);
    int32_t srvId = Bindings.hazardous.findNext(0);

    while (srvId >= 0) {
      auto srv = Bindings.views[srvId].ptr();

      if (likely(srv && srv->TestHazards())) {
        bool hazard = CheckViewOverlap(pView, srv);

        if (unlikely(hazard)) {
          Bindings.views[srvId] = nullptr;
          Bindings.hazardous.clr(srvId);

          BindShaderResource(slotId + srvId, nullptr);
        }
      } else {
        // Avoid further redundant iterations
        Bindings.hazardous.clr(srvId);
      }

      srvId = Bindings.hazardous.findNext(srvId + 1);
    }
  }


  template<typename T>
  void D3D11DeviceContext::ResolveCsSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::ComputeShader>  (pView, m_state.cs.shaderResources);
  }


  template<typename T>
  void D3D11DeviceContext::ResolveOmSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::VertexShader>   (pView, m_state.vs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::HullShader>     (pView, m_state.hs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::DomainShader>   (pView, m_state.ds.shaderResources);
    ResolveSrvHazards<DxbcProgramType::GeometryShader> (pView, m_state.gs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::PixelShader>    (pView, m_state.ps.shaderResources);
  }

  
  bool D3D11DeviceContext::ResolveOmRtvHazards(
          D3D11UnorderedAccessView*         pView) {
    if (!pView || !pView->HasBindFlag(D3D11_BIND_RENDER_TARGET))
      return false;

    bool hazard = false;

    if (CheckViewOverlap(pView, m_state.om.depthStencilView.ptr())) {
      m_state.om.depthStencilView = nullptr;
      hazard = true;
    }

    for (uint32_t i = 0; i < m_state.om.maxRtv; i++) {
      if (CheckViewOverlap(pView, m_state.om.renderTargetViews[i].ptr())) {
        m_state.om.renderTargetViews[i] = nullptr;
        hazard = true;
      }
    }

    return hazard;
  }


  void D3D11DeviceContext::ResolveOmUavHazards(
          D3D11RenderTargetView*            pView) {
    if (!pView || !pView->HasBindFlag(D3D11_BIND_UNORDERED_ACCESS))
      return;

    uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::PixelShader, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::PixelShader, 0);

    for (uint32_t i = 0; i < m_state.om.maxUav; i++) {
      if (CheckViewOverlap(pView, m_state.ps.unorderedAccessViews[i].ptr())) {
        m_state.ps.unorderedAccessViews[i] = nullptr;

        BindUnorderedAccessView(
          uavSlotId + i, nullptr,
          ctrSlotId + i, ~0u);
      }
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
  
  
  VkClearValue D3D11DeviceContext::ConvertColorValue(
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
  

  void D3D11DeviceContext::InitDefaultPrimitiveTopology(
          DxvkInputAssemblyState*           pIaState) {
    pIaState->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    pIaState->primitiveRestart  = VK_FALSE;
    pIaState->patchVertexCount  = 0;
  }


  void D3D11DeviceContext::InitDefaultRasterizerState(
          DxvkRasterizerState*              pRsState) {
    pRsState->polygonMode     = VK_POLYGON_MODE_FILL;
    pRsState->cullMode        = VK_CULL_MODE_BACK_BIT;
    pRsState->frontFace       = VK_FRONT_FACE_CLOCKWISE;
    pRsState->depthClipEnable = VK_TRUE;
    pRsState->depthBiasEnable = VK_FALSE;
    pRsState->sampleCount     = 0;
  }


  void D3D11DeviceContext::InitDefaultDepthStencilState(
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

  
  void D3D11DeviceContext::InitDefaultBlendState(
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

}
