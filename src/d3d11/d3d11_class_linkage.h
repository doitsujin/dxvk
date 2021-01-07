#pragma once

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  // TODO implement properly
  class D3D11ClassLinkage : public D3D11DeviceChild<ID3D11ClassLinkage> {
    
  public:
    
    D3D11ClassLinkage(
            D3D11Device*                pDevice);
    
    ~D3D11ClassLinkage();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE CreateClassInstance(
            LPCSTR              pClassTypeName,
            UINT                ConstantBufferOffset,
            UINT                ConstantVectorOffset,
            UINT                TextureOffset,
            UINT                SamplerOffset,
            ID3D11ClassInstance **ppInstance);
    
    HRESULT STDMETHODCALLTYPE GetClassInstance(
            LPCSTR              pClassInstanceName,
            UINT                InstanceIndex,
            ID3D11ClassInstance **ppInstance);  
    
  };
  
}
