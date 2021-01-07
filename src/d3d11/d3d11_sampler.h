#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_sampler.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11SamplerState : public D3D11StateObject<ID3D11SamplerState> {
    
  public:
    
    using DescType = D3D11_SAMPLER_DESC;
    
    D3D11SamplerState(
            D3D11Device*        device,
      const D3D11_SAMPLER_DESC& desc);
    ~D3D11SamplerState();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_SAMPLER_DESC* pDesc) final;
    
    Rc<DxvkSampler> GetDXVKSampler() const {
      return m_sampler;
    }

    D3D10SamplerState* GetD3D10Iface() {
      return &m_d3d10;
    }
    
    static HRESULT NormalizeDesc(
            D3D11_SAMPLER_DESC* pDesc);
    
  private:
    
    D3D11_SAMPLER_DESC m_desc;
    Rc<DxvkSampler>    m_sampler;
    D3D10SamplerState  m_d3d10;

    std::atomic<uint32_t> m_refCount = { 0u };

    static bool ValidateAddressMode(
            D3D11_TEXTURE_ADDRESS_MODE  Mode);

    static bool ValidateComparisonFunc(
            D3D11_COMPARISON_FUNC       Comparison);
    
  };
  
}
