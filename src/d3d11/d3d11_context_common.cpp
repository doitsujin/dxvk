#include "d3d11_context_common.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"

namespace dxvk {

  template<typename ContextType>
  D3D11CommonContext<ContextType>::D3D11CommonContext(
          D3D11Device*            pParent,
    const Rc<DxvkDevice>&         Device,
          DxvkCsChunkFlags        CsFlags)
  : D3D11DeviceContext(pParent, Device, CsFlags),
    m_contextExt(static_cast<ContextType*>(this)),
    m_annotation(static_cast<ContextType*>(this), Device) {

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

    if (riid == __uuidof(ID3D10Multithread)) {
      *ppvObject = ref(&m_multithread);
      return S_OK;
    }

    Logger::warn("D3D11DeviceContext::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
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
  BOOL STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IsAnnotationEnabled() {
    return m_annotation.GetStatus();
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindShader(
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
    ] (DxvkContext* ctx) mutable {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);

      uint32_t slotId = computeConstantBufferBinding(ShaderStage,
        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

      ctx->bindShader(stage,
        Forwarder::move(cShader));
      ctx->bindResourceBuffer(stage, slotId,
        Forwarder::move(cSlice));
    });
  }


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindFramebuffer() {
    DxvkRenderTargets attachments;
    uint32_t sampleCount = 0;

    // D3D11 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      if (m_state.om.renderTargetViews[i] != nullptr) {
        attachments.color[i] = {
          m_state.om.renderTargetViews[i]->GetImageView(),
          m_state.om.renderTargetViews[i]->GetRenderLayout() };
        sampleCount = m_state.om.renderTargetViews[i]->GetSampleCount();
      }
    }

    if (m_state.om.depthStencilView != nullptr) {
      attachments.depth = {
        m_state.om.depthStencilView->GetImageView(),
        m_state.om.depthStencilView->GetRenderLayout() };
      sampleCount = m_state.om.depthStencilView->GetSampleCount();
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments)
    ] (DxvkContext* ctx) mutable {
      ctx->bindRenderTargets(Forwarder::move(cAttachments));
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
    if (likely(pBuffer != nullptr)) {
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
  void D3D11CommonContext<ContextType>::BindIndexBuffer(
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          DXGI_FORMAT                       Format) {
    VkIndexType indexType = Format == DXGI_FORMAT_R16_UINT
      ? VK_INDEX_TYPE_UINT16
      : VK_INDEX_TYPE_UINT32;

    EmitCs([
      cBufferSlice  = pBuffer != nullptr ? pBuffer->GetBufferSlice(Offset) : DxvkBufferSlice(),
      cIndexType    = indexType
    ] (DxvkContext* ctx) mutable {
      ctx->bindIndexBuffer(
        Forwarder::move(cBufferSlice),
        cIndexType);
    });
  }
  

  template<typename ContextType>
  void D3D11CommonContext<ContextType>::BindXfbBuffer(
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
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindConstantBuffer(
          UINT                              Slot,
          D3D11Buffer*                      pBuffer,
          UINT                              Offset,
          UINT                              Length) {
    EmitCs([
      cSlotId      = Slot,
      cBufferSlice = pBuffer ? pBuffer->GetBufferSlice(16 * Offset, 16 * Length) : DxvkBufferSlice()
    ] (DxvkContext* ctx) mutable {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
      ctx->bindResourceBuffer(stage, cSlotId,
        Forwarder::move(cBufferSlice));
    });
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
      ctx->bindResourceBufferRange(stage, cSlotId, cOffset, cLength);
    });
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindSampler(
          UINT                              Slot,
          D3D11SamplerState*                pSampler) {
    EmitCs([
      cSlotId   = Slot,
      cSampler  = pSampler != nullptr ? pSampler->GetDXVKSampler() : nullptr
    ] (DxvkContext* ctx) mutable {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
      ctx->bindResourceSampler(stage, cSlotId,
        Forwarder::move(cSampler));
    });
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindShaderResource(
          UINT                              Slot,
          D3D11ShaderResourceView*          pResource) {
    EmitCs([
      cSlotId     = Slot,
      cImageView  = pResource != nullptr ? pResource->GetImageView()  : nullptr,
      cBufferView = pResource != nullptr ? pResource->GetBufferView() : nullptr
    ] (DxvkContext* ctx) mutable {
      VkShaderStageFlagBits stage = GetShaderStage(ShaderStage);
      ctx->bindResourceView(stage, cSlotId,
        Forwarder::move(cImageView),
        Forwarder::move(cBufferView));
    });
  }


  template<typename ContextType>
  template<DxbcProgramType ShaderStage>
  void D3D11CommonContext<ContextType>::BindUnorderedAccessView(
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
    ] (DxvkContext* ctx) mutable {
      VkShaderStageFlags stages = ShaderStage == DxbcProgramType::PixelShader
        ? VK_SHADER_STAGE_ALL_GRAPHICS
        : VK_SHADER_STAGE_COMPUTE_BIT;

      if (cCounterSlice.defined() && cCounterValue != ~0u) {
        ctx->updateBuffer(
          cCounterSlice.buffer(),
          cCounterSlice.offset(),
          sizeof(uint32_t),
          &cCounterValue);
      }

      ctx->bindResourceView(stages, cUavSlotId,
        Forwarder::move(cImageView),
        Forwarder::move(cBufferView));
      ctx->bindResourceBuffer(stages, cCtrSlotId,
        Forwarder::move(cCounterSlice));
    });
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


  // Explicitly instantiate here
  template class D3D11CommonContext<D3D11DeferredContext>;
  template class D3D11CommonContext<D3D11ImmediateContext>;

}
