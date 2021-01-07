#include "d3d11_device.h"
#include "d3d11_sampler.h"
#include "d3d11_util.h"

namespace dxvk {
  
  D3D11SamplerState::D3D11SamplerState(
          D3D11Device*        device,
    const D3D11_SAMPLER_DESC& desc)
  : D3D11StateObject<ID3D11SamplerState>(device),
    m_desc(desc), m_d3d10(this) {
    DxvkSamplerCreateInfo info;
    
    // While D3D11_FILTER is technically an enum, its value bits
    // can be used to decode the filter properties more efficiently.
    const uint32_t filterBits = uint32_t(desc.Filter);
    info.magFilter      = (filterBits & 0x04) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.minFilter      = (filterBits & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    
    // Set up the remaining properties, which are
    // stored directly in the sampler description
    info.mipmapMode     = (filterBits & 0x01) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.mipmapLodBias  = desc.MipLODBias;
    info.mipmapLodMin   = desc.MinLOD;
    info.mipmapLodMax   = desc.MaxLOD;
    
    info.useAnisotropy  = (filterBits & 0x40) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy  = float(desc.MaxAnisotropy);
    
    info.addressModeU   = DecodeAddressMode(desc.AddressU);
    info.addressModeV   = DecodeAddressMode(desc.AddressV);
    info.addressModeW   = DecodeAddressMode(desc.AddressW);
    
    info.compareToDepth = (filterBits & 0x80) ? VK_TRUE : VK_FALSE;
    info.compareOp      = DecodeCompareOp(desc.ComparisonFunc);
    
    for (uint32_t i = 0; i < 4; i++)
      info.borderColor.float32[i] = desc.BorderColor[i];
    
    info.usePixelCoord  = VK_FALSE;  // Not supported in D3D11
    
    // Make sure to use a valid anisotropy value
    if (desc.MaxAnisotropy <  1) info.maxAnisotropy =  1.0f;
    if (desc.MaxAnisotropy > 16) info.maxAnisotropy = 16.0f;
    
    // Enforce anisotropy specified in the device options
    int32_t samplerAnisotropyOption = device->GetOptions()->samplerAnisotropy;

    if (samplerAnisotropyOption >= 0) {
      info.useAnisotropy = samplerAnisotropyOption > 0;
      info.maxAnisotropy = float(samplerAnisotropyOption);
    }
    
    m_sampler = device->GetDXVKDevice()->createSampler(info);
  }
  
  
  D3D11SamplerState::~D3D11SamplerState() {
    
  }


  HRESULT STDMETHODCALLTYPE D3D11SamplerState::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11SamplerState)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10SamplerState)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    Logger::warn("D3D11SamplerState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11SamplerState::GetDesc(D3D11_SAMPLER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11SamplerState::NormalizeDesc(D3D11_SAMPLER_DESC* pDesc) {
    const uint32_t filterBits = uint32_t(pDesc->Filter);
    
    if (filterBits & 0xFFFFFF2A) {
      Logger::err(str::format(
        "D3D11SamplerState: Unhandled filter: ", filterBits));
      return E_INVALIDARG;
    }
    
    if (pDesc->MaxAnisotropy <  0
     || pDesc->MaxAnisotropy > 16) {
      return E_INVALIDARG;
    } else if ((filterBits & 0x40) == 0 /* not anisotropic */) {
      // Reset anisotropy if it is not used
      pDesc->MaxAnisotropy = 0;
    }
    
    if (filterBits & 0x80 /* compare-to-depth */) {
      if (!ValidateComparisonFunc(pDesc->ComparisonFunc))
        return E_INVALIDARG;
    } else {
      // Reset compare func if it is not used
      pDesc->ComparisonFunc = D3D11_COMPARISON_NEVER;
    }
    
    if (!ValidateAddressMode(pDesc->AddressU)
     || !ValidateAddressMode(pDesc->AddressV)
     || !ValidateAddressMode(pDesc->AddressW))
      return E_INVALIDARG;
    
    // Clear BorderColor to 0 if none of the address
    // modes are D3D11_TEXTURE_ADDRESS_BORDER
    if (pDesc->AddressU != D3D11_TEXTURE_ADDRESS_BORDER
     && pDesc->AddressV != D3D11_TEXTURE_ADDRESS_BORDER
     && pDesc->AddressW != D3D11_TEXTURE_ADDRESS_BORDER) {
      for (int i = 0; i < 4; i++)
        pDesc->BorderColor[i] = 0.0f;
    }
    
    return S_OK;
  }
  
  
  bool D3D11SamplerState::ValidateAddressMode(D3D11_TEXTURE_ADDRESS_MODE Mode) {
    return Mode >= D3D11_TEXTURE_ADDRESS_WRAP
        && Mode <= D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
  }
  
  
  bool D3D11SamplerState::ValidateComparisonFunc(D3D11_COMPARISON_FUNC Comparison) {
    return Comparison >= D3D11_COMPARISON_NEVER
        && Comparison <= D3D11_COMPARISON_ALWAYS;
  }
  
}
