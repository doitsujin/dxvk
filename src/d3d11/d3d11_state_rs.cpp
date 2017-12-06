#include "d3d11_device.h"
#include "d3d11_state_rs.h"

namespace dxvk {
  
  D3D11RasterizerState::D3D11RasterizerState(
          D3D11Device*                    device,
    const D3D11_RASTERIZER_DESC&          desc)
  : m_device(device), m_desc(desc) {
    
    // Polygon mode. Determines whether the rasterizer fills
    // a polygon or renders lines connecting the vertices.
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    
    switch (desc.FillMode) {
      case D3D11_FILL_WIREFRAME: polygonMode = VK_POLYGON_MODE_LINE; break;
      case D3D11_FILL_SOLID:     polygonMode = VK_POLYGON_MODE_FILL; break;
      
      default:
        Logger::err(str::format(
          "D3D11RasterizerState: Unsupported fill mode: ",
          desc.FillMode));
    }
    
    // Face culling properties. The rasterizer may discard
    // polygons that are facing towards or away from the
    // viewer, depending on the options below.
    VkCullModeFlags cullMode = 0;
    
    switch (desc.CullMode) {
      case D3D11_CULL_NONE:  cullMode = 0;                      break;
      case D3D11_CULL_FRONT: cullMode = VK_CULL_MODE_FRONT_BIT; break;
      case D3D11_CULL_BACK:  cullMode = VK_CULL_MODE_BACK_BIT;  break;
      
      default:
        Logger::err(str::format(
          "D3D11RasterizerState: Unsupported cull mode: ",
          desc.CullMode));
    }
    
    VkFrontFace frontFace = desc.FrontCounterClockwise
      ? VK_FRONT_FACE_COUNTER_CLOCKWISE
      : VK_FRONT_FACE_CLOCKWISE;
    
    // TODO implement depth bias
    if (desc.DepthBias != 0)
      Logger::err("D3D11RasterizerState: Depth bias not supported");
    
    // TODO implement depth clamp
    if (!desc.DepthClipEnable)
      Logger::err("D3D11RasterizerState: Depth clip not supported");
    
    if (desc.AntialiasedLineEnable)
      Logger::err("D3D11RasterizerState: Antialiased lines not supported");
    
    m_state = new DxvkRasterizerState(
      VK_FALSE, VK_FALSE,
      polygonMode, cullMode, frontFace,
      VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);
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
    *ppDevice = m_device.ref();
  }
  
  
  void D3D11RasterizerState::GetDesc(D3D11_RASTERIZER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
}