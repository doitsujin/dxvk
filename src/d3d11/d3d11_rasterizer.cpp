#include "d3d11_device.h"
#include "d3d11_rasterizer.h"

namespace dxvk {
  
  D3D11RasterizerState::D3D11RasterizerState(
          D3D11Device*                    device,
    const D3D11_RASTERIZER_DESC1&         desc)
  : m_device(device), m_desc(desc) {
    
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
    
    // Let's treat the depth bias as enabled by default
    m_state.depthBiasEnable   = VK_TRUE;
    m_state.depthBiasConstant = static_cast<float>(desc.DepthBias);
    m_state.depthBiasClamp    = desc.DepthBiasClamp;
    m_state.depthBiasSlope    = desc.SlopeScaledDepthBias;
    m_state.depthClampEnable  = desc.DepthClipEnable ? VK_FALSE : VK_TRUE;
    
    if (desc.AntialiasedLineEnable)
      Logger::err("D3D11RasterizerState: Antialiased lines not supported");
    
    if (desc.ForcedSampleCount)
      Logger::err("D3D11RasterizerState: Forced sample count not supported");
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