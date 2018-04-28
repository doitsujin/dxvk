#include "d3d11_device.h"
#include "d3d11_sampler.h"
#include "d3d11_util.h"

namespace dxvk {
  
  D3D11SamplerState::D3D11SamplerState(
          D3D11Device*        device,
    const D3D11_SAMPLER_DESC& desc)
  : m_device(device), m_desc(desc) {
    DxvkSamplerCreateInfo info;
    
    // While D3D11_FILTER is technically an enum, its value bits
    // can be used to decode the filter properties more efficiently.
    const uint32_t filterBits = static_cast<uint32_t>(desc.Filter);
    
    info.magFilter      = (filterBits & 0x04) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.minFilter      = (filterBits & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.mipmapMode     = (filterBits & 0x01) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.useAnisotropy  = (filterBits & 0x40) ? VK_TRUE : VK_FALSE;
    info.compareToDepth = (filterBits & 0x80) ? VK_TRUE : VK_FALSE;
    
    // Set up the remaining properties, which are
    // stored directly in the sampler description
    info.mipmapLodBias = desc.MipLODBias;
    info.mipmapLodMin  = desc.MinLOD;
    info.mipmapLodMax  = desc.MaxLOD;
    info.maxAnisotropy = static_cast<float>(desc.MaxAnisotropy);
    info.addressModeU  = DecodeAddressMode(desc.AddressU);
    info.addressModeV  = DecodeAddressMode(desc.AddressV);
    info.addressModeW  = DecodeAddressMode(desc.AddressW);
    info.compareOp     = DecodeCompareOp(desc.ComparisonFunc);
    info.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.usePixelCoord = VK_FALSE;  // Not supported in D3D11
    
    // Make sure to use a valid anisotropy value
    if (desc.MaxAnisotropy <  1) info.maxAnisotropy =  1.0f;
    if (desc.MaxAnisotropy > 16) info.maxAnisotropy = 16.0f;
    
    // Try to find a matching border color if clamp to border is enabled
    if (info.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || info.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || info.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
      info.borderColor = DecodeBorderColor(desc.BorderColor);
    
    m_sampler = device->GetDXVKDevice()->createSampler(info);
  }
  
  
  D3D11SamplerState::~D3D11SamplerState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11SamplerState::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11SamplerState)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11SamplerState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11SamplerState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void STDMETHODCALLTYPE D3D11SamplerState::GetDesc(D3D11_SAMPLER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11SamplerState::NormalizeDesc(D3D11_SAMPLER_DESC* pDesc) {
    const uint32_t filterBits = static_cast<uint32_t>(pDesc->Filter);

    if (filterBits & 0xFFFFFF2A) {
      Logger::err(str::format(
        "D3D11SamplerState: Unhandled filter: ", filterBits));
      return E_INVALIDARG;
    }

    if (pDesc->Filter == D3D11_FILTER_ANISOTROPIC
     || pDesc->Filter == D3D11_FILTER_COMPARISON_ANISOTROPIC) {
      if (pDesc->MaxAnisotropy < 1
        || pDesc->MaxAnisotropy > 16) {
        Logger::err(str::format(
          "D3D11SamplerState: Invalid Anisotropy Range, must be [1,16]: ", pDesc->MaxAnisotropy));
        return E_INVALIDARG;
      }
    } else if (pDesc->MaxAnisotropy < 0
            || pDesc->MaxAnisotropy > 16) {
        Logger::err(str::format(
          "D3D11SamplerState: Invalid Anisotropy Range, must be [0,16]: ", pDesc->MaxAnisotropy));
        return E_INVALIDARG;
    } else
      pDesc->MaxAnisotropy = 0;

    if (IsComparisonFilter(pDesc->Filter))
    {
      if (!ValidComparisonFunc(pDesc->ComparisonFunc)) {
        Logger::err(str::format(
          "D3D11SamplerState: Invalid Comparison Func: ", pDesc->ComparisonFunc));
        return E_INVALIDARG;
      }
    } else {
      pDesc->ComparisonFunc = D3D11_COMPARISON_NEVER;
    }

    if (!ValidAddressMode(pDesc->AddressU)
     || !ValidAddressMode(pDesc->AddressV)
     || !ValidAddressMode(pDesc->AddressW)) {
      Logger::err(str::format(
        "D3D11SamplerState: Invalid Texture Address Mode: ", 
        "\n AddressU: ", pDesc->AddressU, 
        "\n AdressV: ", pDesc->AddressV, 
        "\n AdressW:", pDesc->AddressW ));
      return E_INVALIDARG;
    }
      

    //clear BorderColor to 0 if none of the texture address
    //modes are D3D11_TEXTURE_ADDRESS_BORDER
    if (!(pDesc->AddressU == D3D11_TEXTURE_ADDRESS_BORDER)
     && !(pDesc->AddressV == D3D11_TEXTURE_ADDRESS_BORDER)
     && !(pDesc->AddressW == D3D11_TEXTURE_ADDRESS_BORDER)) {
      for (int i = 0; i < 4; i++) {
         pDesc->BorderColor[i] = 0.0f;
      }
    }
    return S_OK;
  }

  bool D3D11SamplerState::ValidAddressMode(D3D11_TEXTURE_ADDRESS_MODE mode)
  {
    if (mode < D3D11_TEXTURE_ADDRESS_WRAP
     || mode > D3D11_TEXTURE_ADDRESS_MIRROR_ONCE)
      return false;
    return true;
  }

  bool D3D11SamplerState::ValidComparisonFunc(D3D11_COMPARISON_FUNC comparison) {
    if (comparison < D3D11_COMPARISON_NEVER
     || comparison > D3D11_COMPARISON_ALWAYS)
      return false;
    return true;
  }

  bool D3D11SamplerState::IsComparisonFilter(D3D11_FILTER filter) {
    switch (filter) {

    case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT :
    case D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
    case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
    case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
    case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
    case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
    case D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
    case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:
    case D3D11_FILTER_COMPARISON_ANISOTROPIC:
      return true;
    default:
      break;

    }
    return false;
      
  }
  
}
