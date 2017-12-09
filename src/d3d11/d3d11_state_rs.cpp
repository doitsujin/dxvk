#include "d3d11_device.h"
#include "d3d11_state_rs.h"

namespace dxvk {
  
  D3D11RasterizerState::D3D11RasterizerState(
          D3D11Device*                    device,
    const D3D11_RASTERIZER_DESC&          desc)
  : m_device(device), m_desc(desc) {
    
    // State that is not supported in D3D11
    m_state.enableDepthClamp  = VK_FALSE;
    m_state.enableDiscard     = VK_FALSE;
    
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
    
    // TODO implement depth clamp. Note that there are differences
    // between D3D11 depth clip (disabled) and Vulkan depth clamp.
    if (!desc.DepthClipEnable)
      Logger::err("D3D11RasterizerState: Depth clamp not supported");
    
    if (desc.AntialiasedLineEnable)
      Logger::err("D3D11RasterizerState: Antialiased lines not supported");
  }
  
  
  D3D11RasterizerState::~D3D11RasterizerState() {
    
  }
  
  
  HRESULT D3D11RasterizerState::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11RasterizerState);
    
    Logger::warn("D3D11RasterizerState::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11RasterizerState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device;
  }
  
  
  void D3D11RasterizerState::GetDesc(D3D11_RASTERIZER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}