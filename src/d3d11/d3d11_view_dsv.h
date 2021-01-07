#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_view_dsv.h"

#include "d3d11_device_child.h"
#include "d3d11_view.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Depth-stencil view
   * 
   * Unordered access views are special in that they can
   * have counters, which can be used inside shaders to
   * atomically append or consume structures.
   */
  class D3D11DepthStencilView : public D3D11DeviceChild<ID3D11DepthStencilView> {
    
  public:
    
    D3D11DepthStencilView(
            D3D11Device*                      pDevice,
            ID3D11Resource*                   pResource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc);
    
    ~D3D11DepthStencilView();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final;
    
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) final;
    
    void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) final;
    
    const D3D11_VK_VIEW_INFO& GetViewInfo() const {
      return m_info;
    }

    D3D11_RESOURCE_DIMENSION GetResourceType() const {
      D3D11_RESOURCE_DIMENSION type;
      m_resource->GetType(&type);
      return type;
    }
    
    Rc<DxvkImageView> GetImageView() const {
      return m_view;
    }
    
    VkImageLayout GetRenderLayout() const {
      if (m_view->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL) {
        switch (m_desc.Flags & (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL)) {
          default:  // case 0
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
          case D3D11_DSV_READ_ONLY_DEPTH:
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
          case D3D11_DSV_READ_ONLY_STENCIL:
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
          case D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
      } else {
        return VK_IMAGE_LAYOUT_GENERAL;
      }
    }

    VkImageAspectFlags GetWritableAspectMask() const {
      VkImageAspectFlags mask = m_view->formatInfo()->aspectMask;
      if (m_desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)   mask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
      if (m_desc.Flags & D3D11_DSV_READ_ONLY_STENCIL) mask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;
      return mask;
    }

    D3D10DepthStencilView* GetD3D10Iface() {
      return &m_d3d10;
    }
    
    static HRESULT GetDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc);
    
    static HRESULT NormalizeDesc(
            ID3D11Resource*                   pResource,
            D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc);
    
  private:
    
    ID3D11Resource*                   m_resource;
    D3D11_DEPTH_STENCIL_VIEW_DESC     m_desc;
    D3D11_VK_VIEW_INFO                m_info;
    Rc<DxvkImageView>                 m_view;
    D3D10DepthStencilView             m_d3d10;
    
  };
  
}
