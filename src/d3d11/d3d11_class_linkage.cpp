#include "d3d11_class_linkage.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11ClassLinkage::D3D11ClassLinkage(
          D3D11Device*                pDevice)
  : m_device(pDevice) {
    
  }
  
  
  D3D11ClassLinkage::~D3D11ClassLinkage() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11ClassLinkage);
    
    Logger::warn("D3D11ClassLinkage::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11ClassLinkage::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::CreateClassInstance(
          LPCSTR              pClassTypeName,
          UINT                ConstantBufferOffset,
          UINT                ConstantVectorOffset,
          UINT                TextureOffset,
          UINT                SamplerOffset,
          ID3D11ClassInstance **ppInstance) {
    Logger::err("D3D11ClassLinkage::CreateClassInstance: Not implemented yet");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::GetClassInstance(
          LPCSTR              pClassInstanceName,
          UINT                InstanceIndex,
          ID3D11ClassInstance **ppInstance) {
    Logger::err("D3D11ClassLinkage::GetClassInstance: Not implemented yet");
    return E_NOTIMPL;
  }
  
}
