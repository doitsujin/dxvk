#include "d3d11_class_linkage.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11ClassLinkage::D3D11ClassLinkage(
          D3D11Device*                pDevice)
  : D3D11DeviceChild<ID3D11ClassLinkage>(pDevice) {
    
  }
  
  
  D3D11ClassLinkage::~D3D11ClassLinkage() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11ClassLinkage)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11ClassLinkage::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ClassLinkage::CreateClassInstance(
          LPCSTR              pClassTypeName,
          UINT                ConstantBufferOffset,
          UINT                ConstantVectorOffset,
          UINT                TextureOffset,
          UINT                SamplerOffset,
          ID3D11ClassInstance **ppInstance) {
    InitReturnPtr(ppInstance);
    
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
