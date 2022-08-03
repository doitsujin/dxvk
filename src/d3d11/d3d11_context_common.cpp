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
    m_contextExt(GetTypedContext()),
    m_annotation(GetTypedContext(), Device) {

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


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::IASetIndexBuffer(
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

    if (m_state.vs.shader != shader) {
      m_state.vs.shader = shader;

      BindShader<DxbcProgramType::VertexShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::VSGetShader(
          ID3D11VertexShader**              ppVertexShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppVertexShader)
      *ppVertexShader = m_state.vs.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
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

    if (m_state.hs.shader != shader) {
      m_state.hs.shader = shader;

      BindShader<DxbcProgramType::HullShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::HSGetShader(
          ID3D11HullShader**                ppHullShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppHullShader)
      *ppHullShader = m_state.hs.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
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

    if (m_state.ds.shader != shader) {
      m_state.ds.shader = shader;

      BindShader<DxbcProgramType::DomainShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::DSGetShader(
          ID3D11DomainShader**              ppDomainShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppDomainShader)
      *ppDomainShader = m_state.ds.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
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

    if (m_state.gs.shader != shader) {
      m_state.gs.shader = shader;

      BindShader<DxbcProgramType::GeometryShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::GSGetShader(
          ID3D11GeometryShader**            ppGeometryShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppGeometryShader)
      *ppGeometryShader = m_state.gs.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
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

    if (m_state.ps.shader != shader) {
      m_state.ps.shader = shader;

      BindShader<DxbcProgramType::PixelShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::PSGetShader(
          ID3D11PixelShader**               ppPixelShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppPixelShader)
      *ppPixelShader = m_state.ps.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
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

    if (m_state.cs.shader != shader) {
      m_state.cs.shader = shader;

      BindShader<DxbcProgramType::ComputeShader>(GetCommonShader(shader));
    }
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::CSGetShader(
          ID3D11ComputeShader**             ppComputeShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    D3D10DeviceLock lock = LockContext();

    if (ppComputeShader)
      *ppComputeShader = m_state.cs.shader.ref();

    if (pNumClassInstances)
      *pNumClassInstances = 0;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11CommonContext<ContextType>::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    D3D10DeviceLock lock = LockContext();

    if constexpr (!IsDeferred)
      GetTypedContext()->FlushImplicit(true);

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

    if constexpr (!IsDeferred)
      GetTypedContext()->FlushImplicit(true);

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
        ppRenderTargetViews[i] = i < m_state.om.renderTargetViews.size()
          ? m_state.om.renderTargetViews[i].ref()
          : nullptr;
      }
    }

    if (ppDepthStencilView)
      *ppDepthStencilView = m_state.om.depthStencilView.ref();

    if (ppUnorderedAccessViews) {
      for (UINT i = 0; i < NumUAVs; i++) {
        ppUnorderedAccessViews[i] = UAVStartSlot + i < m_state.ps.unorderedAccessViews.size()
          ? m_state.ps.unorderedAccessViews[UAVStartSlot + i].ref()
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
  template<DxbcProgramType ShaderStage, typename T>
  void D3D11CommonContext<ContextType>::ResolveSrvHazards(
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

          BindShaderResource<ShaderStage>(slotId + srvId, nullptr);
        }
      } else {
        // Avoid further redundant iterations
        Bindings.hazardous.clr(srvId);
      }

      srvId = Bindings.hazardous.findNext(srvId + 1);
    }
  }


  template<typename ContextType>
  template<typename T>
  void D3D11CommonContext<ContextType>::ResolveCsSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::ComputeShader>  (pView, m_state.cs.shaderResources);
  }


  template<typename ContextType>
  template<typename T>
  void D3D11CommonContext<ContextType>::ResolveOmSrvHazards(
          T*                                pView) {
    if (!pView) return;
    ResolveSrvHazards<DxbcProgramType::VertexShader>   (pView, m_state.vs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::HullShader>     (pView, m_state.hs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::DomainShader>   (pView, m_state.ds.shaderResources);
    ResolveSrvHazards<DxbcProgramType::GeometryShader> (pView, m_state.gs.shaderResources);
    ResolveSrvHazards<DxbcProgramType::PixelShader>    (pView, m_state.ps.shaderResources);
  }


  template<typename ContextType>
  bool D3D11CommonContext<ContextType>::ResolveOmRtvHazards(
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


  template<typename ContextType>
  void D3D11CommonContext<ContextType>::ResolveOmUavHazards(
          D3D11RenderTargetView*            pView) {
    if (!pView || !pView->HasBindFlag(D3D11_BIND_UNORDERED_ACCESS))
      return;

    uint32_t uavSlotId = computeUavBinding       (DxbcProgramType::PixelShader, 0);
    uint32_t ctrSlotId = computeUavCounterBinding(DxbcProgramType::PixelShader, 0);

    for (uint32_t i = 0; i < m_state.om.maxUav; i++) {
      if (CheckViewOverlap(pView, m_state.ps.unorderedAccessViews[i].ptr())) {
        m_state.ps.unorderedAccessViews[i] = nullptr;

        BindUnorderedAccessView<DxbcProgramType::PixelShader>(
          uavSlotId + i, nullptr,
          ctrSlotId + i, ~0u);
      }
    }
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

    if (needsUpdate)
      BindFramebuffer();
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


  // Explicitly instantiate here
  template class D3D11CommonContext<D3D11DeferredContext>;
  template class D3D11CommonContext<D3D11ImmediateContext>;

}
