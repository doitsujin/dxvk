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
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    HRESULT CreateClassInstance(
            LPCSTR              pClassTypeName,
            UINT                ConstantBufferOffset,
            UINT                ConstantVectorOffset,
            UINT                TextureOffset,
            UINT                SamplerOffset,
            ID3D11ClassInstance **ppInstance);
    
    HRESULT GetClassInstance(
            LPCSTR              pClassInstanceName,
            UINT                InstanceIndex,
            ID3D11ClassInstance **ppInstance);  
    
  private:
    
    Com<D3D11Device> m_device;
    
  };
  
}
