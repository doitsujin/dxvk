#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  static DxgiFormatMode GetFormatModeFromBindFlags(UINT BindFlags) {
    if (BindFlags & D3D11_BIND_RENDER_TARGET)
      return DxgiFormatMode::Color;
    
    if (BindFlags & D3D11_BIND_DEPTH_STENCIL)
      return DxgiFormatMode::Depth;
    
    return DxgiFormatMode::Any;
  }
  
  
  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
    const D3D11_TEXTURE2D_DESC*       pDesc)
  : m_device    (pDevice),
    m_formatMode(GetFormatModeFromBindFlags(pDesc->BindFlags)),
    m_desc      (*pDesc) {
    
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_2D;
    info.format         = pDevice->LookupFormat(
      pDesc->Format, m_formatMode).actual;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = pDesc->Width;
    info.extent.height  = pDesc->Height;
    info.extent.depth   = 1;
    info.numLayers      = pDesc->ArraySize;
    info.mipLevels      = pDesc->MipLevels;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_GENERAL;
    
    if (FAILED(GetSampleCount(pDesc->SampleDesc.Count, &info.sampleCount)))
      throw DxvkError(str::format("D3D11: Invalid sample count: ", pDesc->SampleDesc.Count));
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      info.stages |= pDevice->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_RENDER_TARGET) {
      info.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      info.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      info.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                  |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL) {
      info.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      info.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                  |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      info.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                  |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      info.stages |= pDevice->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags != 0) {
      info.tiling  = VK_IMAGE_TILING_LINEAR;
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      
      if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        info.access |= VK_ACCESS_HOST_WRITE_BIT;
      
      if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ)
        info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (pDesc->MipLevels == 0)
      info.mipLevels = util::computeMipLevelCount(info.extent);
    
    m_image = pDevice->GetDXVKDevice()->createImage(
      info, GetMemoryFlagsForUsage(pDesc->Usage));
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Texture2D);
    
    Logger::warn("D3D11Texture2D::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
    
  void STDMETHODCALLTYPE D3D11Texture2D::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture2D::GetEvictionPriority() {
    Logger::warn("D3D11Texture2D::GetEvictionPriority: Stub");
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::SetEvictionPriority(UINT EvictionPriority) {
    Logger::warn("D3D11Texture2D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetDesc(D3D11_TEXTURE2D_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  
  HRESULT GetCommonTextureInfo(
          ID3D11Resource*   pResource,
          D3D11TextureInfo* pTextureInfo) {
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&dimension);
    
    switch (dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        auto tex = static_cast<D3D11Texture2D*>(pResource);
        pTextureInfo->formatMode = tex->GetFormatMode();
        pTextureInfo->image      = tex->GetDXVKImage();
      } return S_OK;
      
      default:
        return E_INVALIDARG;
    }
  }
  
}
