#pragma once

#include "../dxvk/dxvk_device.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Render target view
   */
  class D3D11RenderTargetView : public D3D11DeviceChild<ID3D11RenderTargetView> {
    
  public:
    
    D3D11RenderTargetView(
            D3D11Device*                      device,
            ID3D11Resource*                   resource,
      const D3D11_RENDER_TARGET_VIEW_DESC&    desc,
      const Rc<DxvkImageView>&                view);
    
    ~D3D11RenderTargetView();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) final;
    
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) final;
    
    void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) final;
    
    D3D11_RESOURCE_DIMENSION GetResourceType() const {
      D3D11_RESOURCE_DIMENSION type;
      m_resource->GetType(&type);
      return type;
    }
    
    Rc<DxvkImageView> GetImageView() const {
      return m_view;
    }
    
    VkImageLayout GetRenderLayout() const {
      return m_view->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }
    
    static HRESULT GetDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC*    pDesc);
    
    static HRESULT NormalizeDesc(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC*    pDesc);
    
  private:
    
    Com<D3D11Device>                  m_device;
    Com<ID3D11Resource>               m_resource;
    D3D11_RENDER_TARGET_VIEW_DESC     m_desc;
    Rc<DxvkImageView>                 m_view;
    
  };
  
}
