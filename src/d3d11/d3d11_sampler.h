#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11SamplerState : public D3D11DeviceChild<ID3D11SamplerState> {
    
  public:
    
    using DescType = D3D11_SAMPLER_DESC;
    
    D3D11SamplerState(
            D3D11Device*        device,
      const D3D11_SAMPLER_DESC& desc);
    ~D3D11SamplerState();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_SAMPLER_DESC* pDesc) final;
    
    Rc<DxvkSampler> GetDXVKSampler() const {
      return m_sampler;
    }
    
    static HRESULT ValidateDesc(
      const D3D11_SAMPLER_DESC* desc);
    
  private:
    
    Com<D3D11Device>   m_device;
    D3D11_SAMPLER_DESC m_desc;
    Rc<DxvkSampler>    m_sampler;
    
  };
  
}
