#include "d3d10_state_block.h"

#define MAKE_STATE_TYPE(field, count) { offsetof(D3D10_STATE_BLOCK_MASK, field), count }

namespace dxvk {

  static const std::array<std::pair<size_t, size_t>, 24> g_stateTypes = {{
    MAKE_STATE_TYPE(SOBuffers,                1),
    MAKE_STATE_TYPE(OMRenderTargets,          1),
    MAKE_STATE_TYPE(OMDepthStencilState,      1),
    MAKE_STATE_TYPE(OMBlendState,             1),
    MAKE_STATE_TYPE(VS,                       1),
    MAKE_STATE_TYPE(VSSamplers,               D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT),
    MAKE_STATE_TYPE(VSShaderResources,        D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT),
    MAKE_STATE_TYPE(VSConstantBuffers,        D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT),
    MAKE_STATE_TYPE(GS,                       1),
    MAKE_STATE_TYPE(GSSamplers,               D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT),
    MAKE_STATE_TYPE(GSShaderResources,        D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT),
    MAKE_STATE_TYPE(GSConstantBuffers,        D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT),
    MAKE_STATE_TYPE(PS,                       1),
    MAKE_STATE_TYPE(PSSamplers,               D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT),
    MAKE_STATE_TYPE(PSShaderResources,        D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT),
    MAKE_STATE_TYPE(PSConstantBuffers,        D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT),
    MAKE_STATE_TYPE(IAVertexBuffers,          D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT),
    MAKE_STATE_TYPE(IAIndexBuffer,            1),
    MAKE_STATE_TYPE(IAInputLayout,            1),
    MAKE_STATE_TYPE(IAPrimitiveTopology,      1),
    MAKE_STATE_TYPE(RSViewports,              1),
    MAKE_STATE_TYPE(RSScissorRects,           1),
    MAKE_STATE_TYPE(RSRasterizerState,        1),
    MAKE_STATE_TYPE(Predication,              1),
  }};


  D3D10StateBlock::D3D10StateBlock(
          ID3D10Device*             pDevice,
    const D3D10_STATE_BLOCK_MASK*   pMask)
  : m_device(pDevice), m_mask(*pMask) {

  }

  
  D3D10StateBlock::~D3D10StateBlock() {

  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlock::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;
    
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D10StateBlock)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D10StateBlock::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlock::Capture() {
    m_state = D3D10_STATE_BLOCK_STATE();

    if (TestBit(&m_mask.VS, 0)) m_device->VSGetShader(&m_state.vs);
    if (TestBit(&m_mask.GS, 0)) m_device->GSGetShader(&m_state.gs);
    if (TestBit(&m_mask.PS, 0)) m_device->PSGetShader(&m_state.ps);
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSSamplers, i)) m_device->VSGetSamplers(i, 1, &m_state.vsSso[i]);
      if (TestBit(m_mask.GSSamplers, i)) m_device->GSGetSamplers(i, 1, &m_state.gsSso[i]);
      if (TestBit(m_mask.PSSamplers, i)) m_device->PSGetSamplers(i, 1, &m_state.psSso[i]);
    }
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSShaderResources, i)) m_device->VSGetShaderResources(i, 1, &m_state.vsSrv[i]);
      if (TestBit(m_mask.GSShaderResources, i)) m_device->GSGetShaderResources(i, 1, &m_state.gsSrv[i]);
      if (TestBit(m_mask.PSShaderResources, i)) m_device->PSGetShaderResources(i, 1, &m_state.psSrv[i]);
    }
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSConstantBuffers, i)) m_device->VSGetConstantBuffers(i, 1, &m_state.vsCbo[i]);
      if (TestBit(m_mask.GSConstantBuffers, i)) m_device->GSGetConstantBuffers(i, 1, &m_state.gsCbo[i]);
      if (TestBit(m_mask.PSConstantBuffers, i)) m_device->PSGetConstantBuffers(i, 1, &m_state.psCbo[i]);
    }

    for (uint32_t i = 0; i < D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
      if (TestBit(m_mask.IAVertexBuffers, i)) {
        m_device->IAGetVertexBuffers(i, 1,
          &m_state.iaVertexBuffers[i],
          &m_state.iaVertexOffsets[i],
          &m_state.iaVertexStrides[i]);
      }
    }

    if (TestBit(&m_mask.IAIndexBuffer, 0)) {
      m_device->IAGetIndexBuffer(
        &m_state.iaIndexBuffer,
        &m_state.iaIndexFormat,
        &m_state.iaIndexOffset);
    }

    if (TestBit(&m_mask.IAInputLayout, 0))
      m_device->IAGetInputLayout(&m_state.iaInputLayout);
    
    if (TestBit(&m_mask.IAPrimitiveTopology, 0))
      m_device->IAGetPrimitiveTopology(&m_state.iaTopology);
    
    if (TestBit(&m_mask.OMRenderTargets, 0)) {
      m_device->OMGetRenderTargets(
        D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT,
        &m_state.omRtv[0], &m_state.omDsv);
    }

    if (TestBit(&m_mask.OMDepthStencilState, 0)) {
      m_device->OMGetDepthStencilState(
        &m_state.omDepthStencilState,
        &m_state.omStencilRef);
    }
    
    if (TestBit(&m_mask.OMBlendState, 0)) {
      m_device->OMGetBlendState(
        &m_state.omBlendState,
         m_state.omBlendFactor,
        &m_state.omSampleMask);
    }
    
    if (TestBit(&m_mask.RSViewports, 0)) {
      m_device->RSGetViewports(&m_state.rsViewportCount, nullptr);
      m_device->RSGetViewports(&m_state.rsViewportCount, m_state.rsViewports);
    }
    
    if (TestBit(&m_mask.RSScissorRects, 0)) {
      m_device->RSGetScissorRects(&m_state.rsScissorCount, nullptr);
      m_device->RSGetScissorRects(&m_state.rsScissorCount, m_state.rsScissors);
    }

    if (TestBit(&m_mask.RSRasterizerState, 0))
      m_device->RSGetState(&m_state.rsState);
    
    if (TestBit(&m_mask.SOBuffers, 0)) {
      m_device->SOGetTargets(
        D3D10_SO_BUFFER_SLOT_COUNT,
        &m_state.soBuffers[0],
        &m_state.soOffsets[0]);
    }

    if (TestBit(&m_mask.Predication, 0))
      m_device->GetPredication(&m_state.predicate, &m_state.predicateInvert);

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlock::Apply() {
    if (TestBit(&m_mask.VS, 0)) m_device->VSSetShader(m_state.vs.ptr());
    if (TestBit(&m_mask.GS, 0)) m_device->GSSetShader(m_state.gs.ptr());
    if (TestBit(&m_mask.PS, 0)) m_device->PSSetShader(m_state.ps.ptr());
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSSamplers, i)) m_device->VSSetSamplers(i, 1, &m_state.vsSso[i]);
      if (TestBit(m_mask.GSSamplers, i)) m_device->GSSetSamplers(i, 1, &m_state.gsSso[i]);
      if (TestBit(m_mask.PSSamplers, i)) m_device->PSSetSamplers(i, 1, &m_state.psSso[i]);
    }
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSShaderResources, i)) m_device->VSSetShaderResources(i, 1, &m_state.vsSrv[i]);
      if (TestBit(m_mask.GSShaderResources, i)) m_device->GSSetShaderResources(i, 1, &m_state.gsSrv[i]);
      if (TestBit(m_mask.PSShaderResources, i)) m_device->PSSetShaderResources(i, 1, &m_state.psSrv[i]);
    }
    
    for (uint32_t i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
      if (TestBit(m_mask.VSConstantBuffers, i)) m_device->VSSetConstantBuffers(i, 1, &m_state.vsCbo[i]);
      if (TestBit(m_mask.GSConstantBuffers, i)) m_device->GSSetConstantBuffers(i, 1, &m_state.gsCbo[i]);
      if (TestBit(m_mask.PSConstantBuffers, i)) m_device->PSSetConstantBuffers(i, 1, &m_state.psCbo[i]);
    }

    for (uint32_t i = 0; i < D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
      if (TestBit(m_mask.IAVertexBuffers, i)) {
        m_device->IASetVertexBuffers(i, 1,
          &m_state.iaVertexBuffers[i],
          &m_state.iaVertexOffsets[i],
          &m_state.iaVertexStrides[i]);
      }
    }

    if (TestBit(&m_mask.IAIndexBuffer, 0)) {
      m_device->IASetIndexBuffer(
        m_state.iaIndexBuffer.ptr(),
        m_state.iaIndexFormat,
        m_state.iaIndexOffset);
    }

    if (TestBit(&m_mask.IAInputLayout, 0))
      m_device->IASetInputLayout(m_state.iaInputLayout.ptr());
    
    if (TestBit(&m_mask.IAPrimitiveTopology, 0))
      m_device->IASetPrimitiveTopology(m_state.iaTopology);
    
    if (TestBit(&m_mask.OMRenderTargets, 0)) {
      m_device->OMSetRenderTargets(
        D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT,
        &m_state.omRtv[0], m_state.omDsv.ptr());
    }

    if (TestBit(&m_mask.OMDepthStencilState, 0)) {
      m_device->OMSetDepthStencilState(
        m_state.omDepthStencilState.ptr(),
        m_state.omStencilRef);
    }
    
    if (TestBit(&m_mask.OMBlendState, 0)) {
      m_device->OMSetBlendState(
        m_state.omBlendState.ptr(),
        m_state.omBlendFactor,
        m_state.omSampleMask);
    }
    
    if (TestBit(&m_mask.RSViewports, 0))
      m_device->RSSetViewports(m_state.rsViewportCount, m_state.rsViewports);
    
    if (TestBit(&m_mask.RSScissorRects, 0))
      m_device->RSSetScissorRects(m_state.rsScissorCount, m_state.rsScissors);

    if (TestBit(&m_mask.RSRasterizerState, 0))
      m_device->RSSetState(m_state.rsState.ptr());
    
    if (TestBit(&m_mask.SOBuffers, 0)) {
      m_device->SOSetTargets(
        D3D10_SO_BUFFER_SLOT_COUNT,
        &m_state.soBuffers[0],
        &m_state.soOffsets[0]);
    }
    
    if (TestBit(&m_mask.Predication, 0))
      m_device->SetPredication(m_state.predicate.ptr(), m_state.predicateInvert);

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlock::GetDevice(
          ID3D10Device**            ppDevice) {
    Logger::err("D3D10StateBlock::GetDevice: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlock::ReleaseAllDeviceObjects() {
    // Not entirely sure if this is correct?
    m_state = D3D10_STATE_BLOCK_STATE();
    return S_OK;
  }


  BOOL D3D10StateBlock::TestBit(
    const BYTE*                     pMask,
          UINT                      Idx) {
    uint32_t byte = Idx / 8;
    uint32_t bit  = Idx % 8;
    return (pMask[byte] & (1 << bit)) != 0;
  }

}

extern "C" {
  using namespace dxvk;

  HRESULT STDMETHODCALLTYPE D3D10CreateStateBlock(
          ID3D10Device*             pDevice,
          D3D10_STATE_BLOCK_MASK*   pStateBlockMask,
          ID3D10StateBlock**        ppStateBlock) {
    InitReturnPtr(ppStateBlock);

    if (!pDevice || !pStateBlockMask || !ppStateBlock)
      return E_INVALIDARG;
    
    *ppStateBlock = ref(new D3D10StateBlock(pDevice, pStateBlockMask));
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskEnableCapture(
          D3D10_STATE_BLOCK_MASK*   pMask,
          D3D10_DEVICE_STATE_TYPES  StateType,
          UINT                      StartIdx,
          UINT                      Count) {
    if (!pMask || !StateType || StateType > g_stateTypes.size())
      return E_INVALIDARG;
    
    auto pair = g_stateTypes[uint32_t(StateType) - 1];
    auto mask = reinterpret_cast<BYTE*>(pMask) + pair.first;

    if (StartIdx + Count > pair.second)
      return E_INVALIDARG;
    
    for (uint32_t i = StartIdx; i < StartIdx + Count; i++) {
      uint32_t byte = i / 8;
      uint32_t bit  = i % 8;
      mask[byte] |= 1 << bit;
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskDisableCapture(
          D3D10_STATE_BLOCK_MASK*   pMask,
          D3D10_DEVICE_STATE_TYPES  StateType,
          UINT                      StartIdx,
          UINT                      Count) {
    if (!pMask || !StateType || StateType > g_stateTypes.size())
      return E_INVALIDARG;
    
    auto pair = g_stateTypes[uint32_t(StateType) - 1];
    auto mask = reinterpret_cast<BYTE*>(pMask) + pair.first;

    if (StartIdx + Count > pair.second)
      return E_INVALIDARG;
    
    for (uint32_t i = StartIdx; i < StartIdx + Count; i++) {
      uint32_t byte = i / 8;
      uint32_t bit  = i % 8;
      mask[byte] &= ~(1 << bit);
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskEnableAll(
          D3D10_STATE_BLOCK_MASK*   pMask) {
    if (!pMask)
      return E_INVALIDARG;
    
    *pMask = D3D10_STATE_BLOCK_MASK();
    for (size_t i = 0; i < g_stateTypes.size(); i++) {
      D3D10StateBlockMaskEnableCapture(pMask,
        D3D10_DEVICE_STATE_TYPES(i + 1),
        0, g_stateTypes[i].second);
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskDisableAll(
          D3D10_STATE_BLOCK_MASK* pMask) {
    if (!pMask)
      return E_INVALIDARG;
    
    *pMask = D3D10_STATE_BLOCK_MASK();
    return S_OK;
  }


  BOOL STDMETHODCALLTYPE D3D10StateBlockMaskGetSetting(
          D3D10_STATE_BLOCK_MASK*   pMask,
          D3D10_DEVICE_STATE_TYPES  StateType,
          UINT                      Idx) {
    if (!pMask || !StateType || StateType > g_stateTypes.size())
      return FALSE;
    
    auto pair = g_stateTypes[uint32_t(StateType) - 1];
    auto mask = reinterpret_cast<BYTE*>(pMask) + pair.first;

    if (Idx >= pair.second)
      return FALSE;
    
    uint32_t byte = Idx / 8;
    uint32_t bit  = Idx % 8;
    return (mask[byte] & (1 << bit)) != 0;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskDifference(
          D3D10_STATE_BLOCK_MASK* pA,
          D3D10_STATE_BLOCK_MASK* pB,
          D3D10_STATE_BLOCK_MASK* pResult) {
    if (!pA || !pB || !pResult)
      return E_INVALIDARG;
    
    auto a = reinterpret_cast<const BYTE*>(pA);
    auto b = reinterpret_cast<const BYTE*>(pB);
    auto r = reinterpret_cast<BYTE*>(pResult);

    for (size_t i = 0; i < sizeof(D3D10_STATE_BLOCK_MASK); i++)
      r[i] = a[i] ^ b[i];
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskIntersect(
          D3D10_STATE_BLOCK_MASK*   pA,
          D3D10_STATE_BLOCK_MASK*   pB,
          D3D10_STATE_BLOCK_MASK*   pResult) {
    if (!pA || !pB || !pResult)
      return E_INVALIDARG;
    
    auto a = reinterpret_cast<const BYTE*>(pA);
    auto b = reinterpret_cast<const BYTE*>(pB);
    auto r = reinterpret_cast<BYTE*>(pResult);

    for (size_t i = 0; i < sizeof(D3D10_STATE_BLOCK_MASK); i++)
      r[i] = a[i] & b[i];
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10StateBlockMaskUnion(
          D3D10_STATE_BLOCK_MASK*   pA,
          D3D10_STATE_BLOCK_MASK*   pB,
          D3D10_STATE_BLOCK_MASK*   pResult) {
    if (!pA || !pB || !pResult)
      return E_INVALIDARG;
    
    auto a = reinterpret_cast<const BYTE*>(pA);
    auto b = reinterpret_cast<const BYTE*>(pB);
    auto r = reinterpret_cast<BYTE*>(pResult);

    for (size_t i = 0; i < sizeof(D3D10_STATE_BLOCK_MASK); i++)
      r[i] = a[i] | b[i];
    return S_OK;
  }

}
