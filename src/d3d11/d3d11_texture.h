#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11Texture2D : public D3D11DeviceChild<ID3D11Texture2D> {
    
  public:
    
    D3D11Texture2D(
            D3D11Device*                pDevice,
      const D3D11_TEXTURE2D_DESC*       pDesc);
    
    ~D3D11Texture2D();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE GetType(
            D3D11_RESOURCE_DIMENSION *pResourceDimension) final;
    
    UINT STDMETHODCALLTYPE GetEvictionPriority() final;
    
    void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final;
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_TEXTURE2D_DESC *pDesc) final;
    
    DxgiFormatMode GetFormatMode() const {
      return m_formatMode;
    }
    
    Rc<DxvkImage> GetDXVKImage() const {
      return m_image;
    }
    
  private:
    
    Com<D3D11Device>                m_device;
    DxgiFormatMode                  m_formatMode;
    D3D11_TEXTURE2D_DESC            m_desc;
    Rc<DxvkImage>                   m_image;
    
  };
  
  
  /**
   * \brief Common texture info
   * 
   * Stores the image and the image format
   * mode for a texture of any type.
   */
  struct D3D11TextureInfo {
    DxgiFormatMode  formatMode;
    Rc<DxvkImage>   image;
  };
  
  
  /**
   * \brief Retrieves common info about a texture
   * 
   * \param [in] pResource The resource. Must be a texture.
   * \param [out] pTextureInfo Pointer to the texture info struct.
   * \returns E_INVALIDARG if the resource is not a texture
   */
  HRESULT GetCommonTextureInfo(
          ID3D11Resource*   pResource,
          D3D11TextureInfo* pTextureInfo);
  
}
