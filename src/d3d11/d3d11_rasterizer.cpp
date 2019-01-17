#include "d3d11_device.h"
#include "d3d11_rasterizer.h"

namespace dxvk {
  
  D3D11RasterizerState::D3D11RasterizerState(
          D3D11Device*                    device,
    const D3D11_RASTERIZER_DESC1&         desc)
  : m_device(device), m_desc(desc), m_d3d10(this) {
    // Polygon mode. Determines whether the rasterizer fills
    // a polygon or renders lines connecting the vertices.
    m_state.polygonMode = VK_POLYGON_MODE_FILL;
    
    switch (desc.FillMode) {
      case D3D11_FILL_WIREFRAME: m_state.polygonMode = VK_POLYGON_MODE_LINE; break;
      case D3D11_FILL_SOLID:     m_state.polygonMode = VK_POLYGON_MODE_FILL; break;
      
      default:
        Logger::err(str::format(
          "D3D11RasterizerState: Unsupported fill mode: ",
          desc.FillMode));
    }
    
    // Face culling properties. The rasterizer may discard
    // polygons that are facing towards or away from the
    // viewer, depending on the options below.
    m_state.cullMode = VK_CULL_MODE_NONE;
    
    switch (desc.CullMode) {
      case D3D11_CULL_NONE:  m_state.cullMode = VK_CULL_MODE_NONE;      break;
      case D3D11_CULL_FRONT: m_state.cullMode = VK_CULL_MODE_FRONT_BIT; break;
      case D3D11_CULL_BACK:  m_state.cullMode = VK_CULL_MODE_BACK_BIT;  break;
      
      default:
        Logger::err(str::format(
          "D3D11RasterizerState: Unsupported cull mode: ",
          desc.CullMode));
    }
    
    m_state.frontFace = desc.FrontCounterClockwise
      ? VK_FRONT_FACE_COUNTER_CLOCKWISE
      : VK_FRONT_FACE_CLOCKWISE;
    
    // In the backend we treat depth bias as a dynamic state because
    // some games like to put random/uninitialized numbers here, but
    // we do not need to enable it in case the parameters are both 0.
    m_state.depthBiasEnable   = desc.DepthBias != 0 || desc.SlopeScaledDepthBias != 0.0f;
    m_state.depthClipEnable   = desc.DepthClipEnable;
    m_state.sampleCount       = VkSampleCountFlags(desc.ForcedSampleCount);

    m_depthBias.depthBiasConstant = float(desc.DepthBias);
    m_depthBias.depthBiasSlope    = desc.SlopeScaledDepthBias;
    m_depthBias.depthBiasClamp    = desc.DepthBiasClamp;
    
    if (desc.AntialiasedLineEnable)
      Logger::err("D3D11RasterizerState: Antialiased lines not supported");
  }
  
  
  D3D11RasterizerState::~D3D11RasterizerState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11RasterizerState::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11RasterizerState)
     || riid == __uuidof(ID3D11RasterizerState1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10RasterizerState)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    Logger::warn("D3D11RasterizerState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11RasterizerState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void STDMETHODCALLTYPE D3D11RasterizerState::GetDesc(D3D11_RASTERIZER_DESC* pDesc) {
    pDesc->FillMode               = m_desc.FillMode;
    pDesc->CullMode               = m_desc.CullMode;
    pDesc->FrontCounterClockwise  = m_desc.FrontCounterClockwise;
    pDesc->DepthBias              = m_desc.DepthBias;
    pDesc->DepthBiasClamp         = m_desc.DepthBiasClamp;
    pDesc->SlopeScaledDepthBias   = m_desc.SlopeScaledDepthBias;
    pDesc->DepthClipEnable        = m_desc.DepthClipEnable;
    pDesc->ScissorEnable          = m_desc.ScissorEnable;
    pDesc->MultisampleEnable      = m_desc.MultisampleEnable;
    pDesc->AntialiasedLineEnable  = m_desc.AntialiasedLineEnable;
  }
  
  
  void STDMETHODCALLTYPE D3D11RasterizerState::GetDesc1(D3D11_RASTERIZER_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  void D3D11RasterizerState::BindToContext(const Rc<DxvkContext>& ctx) {
    ctx->setRasterizerState(m_state);
    
    if (m_state.depthBiasEnable)
      ctx->setDepthBias(m_depthBias);
  }
  
  
  D3D11_RASTERIZER_DESC1 D3D11RasterizerState::DefaultDesc() {
    D3D11_RASTERIZER_DESC1 dstDesc;
    dstDesc.FillMode              = D3D11_FILL_SOLID;
    dstDesc.CullMode              = D3D11_CULL_BACK;
    dstDesc.FrontCounterClockwise = FALSE;
    dstDesc.DepthBias             = 0;
    dstDesc.SlopeScaledDepthBias  = 0.0f;
    dstDesc.DepthBiasClamp        = 0.0f;
    dstDesc.DepthClipEnable       = TRUE;
    dstDesc.ScissorEnable         = FALSE;
    dstDesc.MultisampleEnable     = FALSE;
    dstDesc.AntialiasedLineEnable = FALSE;
    dstDesc.ForcedSampleCount     = 0;
    return dstDesc;
  }
  
  
  D3D11_RASTERIZER_DESC1 D3D11RasterizerState::PromoteDesc(
    const D3D11_RASTERIZER_DESC*  pSrcDesc) {
    D3D11_RASTERIZER_DESC1 dstDesc;
    dstDesc.FillMode              = pSrcDesc->FillMode;
    dstDesc.CullMode              = pSrcDesc->CullMode;
    dstDesc.FrontCounterClockwise = pSrcDesc->FrontCounterClockwise;
    dstDesc.DepthBias             = pSrcDesc->DepthBias;
    dstDesc.DepthBiasClamp        = pSrcDesc->DepthBiasClamp;
    dstDesc.SlopeScaledDepthBias  = pSrcDesc->SlopeScaledDepthBias;
    dstDesc.DepthClipEnable       = pSrcDesc->DepthClipEnable;
    dstDesc.ScissorEnable         = pSrcDesc->ScissorEnable;
    dstDesc.MultisampleEnable     = pSrcDesc->MultisampleEnable;
    dstDesc.AntialiasedLineEnable = pSrcDesc->AntialiasedLineEnable;
    dstDesc.ForcedSampleCount     = 0;
    return dstDesc;
  }
  
  
  HRESULT D3D11RasterizerState::NormalizeDesc(
          D3D11_RASTERIZER_DESC1* pDesc) {
    if (pDesc->FillMode < D3D11_FILL_WIREFRAME
     || pDesc->FillMode > D3D11_FILL_SOLID)
      return E_INVALIDARG;
    
    if (pDesc->CullMode < D3D11_CULL_NONE
     || pDesc->CullMode > D3D11_CULL_BACK)
      return E_INVALIDARG;
    
    if (pDesc->FrontCounterClockwise)
      pDesc->FrontCounterClockwise = TRUE;
    
    if (pDesc->DepthClipEnable)
      pDesc->DepthClipEnable = TRUE;
    
    if (pDesc->ScissorEnable)
      pDesc->ScissorEnable = TRUE;
    
    if (pDesc->MultisampleEnable)
      pDesc->MultisampleEnable = TRUE;
    
    if (pDesc->AntialiasedLineEnable)
      pDesc->AntialiasedLineEnable = TRUE;
    
    if (pDesc->ForcedSampleCount != 0) {
      if (FAILED(DecodeSampleCount(pDesc->ForcedSampleCount, nullptr)))
        return E_INVALIDARG;
    }
    
    return S_OK;
  }
  
}