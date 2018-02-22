#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Common texture info
   * 
   * Stores the image and the image format
   * mode for a texture of any type.
   */
  struct D3D11TextureInfo {
    DxgiFormatMode    formatMode;
    Rc<DxvkBuffer>    imageBuffer;
    Rc<DxvkImage>     image;
    
    D3D11_USAGE       usage;
    UINT              bindFlags;
    
    VkImageSubresource mappedSubresource = {
      VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
  };
  
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 1 D
  class D3D11Texture1D : public D3D11DeviceChild<ID3D11Texture1D> {
    
  public:
    
    D3D11Texture1D(
            D3D11Device*                pDevice,
      const D3D11_TEXTURE1D_DESC*       pDesc);
    
    ~D3D11Texture1D();
    
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
            D3D11_TEXTURE1D_DESC *pDesc) final;
    
    D3D11TextureInfo* GetTextureInfo() {
      return &m_texInfo;
    }
    
  private:
    
    Com<D3D11Device>                m_device;
    D3D11_TEXTURE1D_DESC            m_desc;
    D3D11TextureInfo                m_texInfo;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 2 D
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
    
    D3D11TextureInfo* GetTextureInfo() {
      return &m_texInfo;
    }
    
  private:
    
    Com<D3D11Device>                m_device;
    D3D11_TEXTURE2D_DESC            m_desc;
    D3D11TextureInfo                m_texInfo;
    
  };
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  class D3D11Texture3D : public D3D11DeviceChild<ID3D11Texture3D> {
    
  public:
    
    D3D11Texture3D(
            D3D11Device*                pDevice,
      const D3D11_TEXTURE3D_DESC*       pDesc);
    
    ~D3D11Texture3D();
    
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
            D3D11_TEXTURE3D_DESC *pDesc) final;
    
    D3D11TextureInfo* GetTextureInfo() {
      return &m_texInfo;
    }
    
  private:
    
    Com<D3D11Device>                m_device;
    D3D11_TEXTURE3D_DESC            m_desc;
    D3D11TextureInfo                m_texInfo;
    
  };
  
  
  /**
   * \brief Retrieves common info about a texture
   * 
   * \param [in] pResource The resource. Must be a texture.
   * \param [out] pTextureInfo Pointer to the texture info struct.
   * \returns E_INVALIDARG if the resource is not a texture
   */
  D3D11TextureInfo* GetCommonTextureInfo(
          ID3D11Resource*       pResource);
  
  /**
   * \brief Computes image subresource from subresource index
   * 
   * \param [in] Aspect Image aspect mask
   * \param [in] MipLevels Total number of mip levels that the image has
   * \param [in] Subresource The D3D11 subresource index
   * \returns Vulkan image subresource info
   */
  VkImageSubresource GetSubresourceFromIndex(
          VkImageAspectFlags    Aspect,
          UINT                  MipLevels,
          UINT                  Subresource);
  
}
