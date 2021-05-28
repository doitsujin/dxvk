#pragma once

#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_view_rtv.h"

#include "d3d11_device_child.h"
#include "d3d11_view.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Render target view
   */
  class D3D11RenderTargetView : public D3D11DeviceChild<ID3D11RenderTargetView1> {
    
  public:
    
    D3D11RenderTargetView(
            D3D11Device*                      pDevice,
            ID3D11Resource*                   pResource,
      const D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc);
    
    ~D3D11RenderTargetView();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final;
    
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) final;
    
    void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) final;

    void STDMETHODCALLTYPE GetDesc1(D3D11_RENDER_TARGET_VIEW_DESC1* pDesc) final;

    const D3D11_VK_VIEW_INFO& GetViewInfo() const {
      return m_info;
    }

    BOOL HasBindFlag(UINT Flags) const {
      return m_info.BindFlags & Flags;
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
      return m_view->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    D3D10RenderTargetView* GetD3D10Iface() {
      return &m_d3d10;
    }

    static HRESULT GetDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc);
    
    static D3D11_RENDER_TARGET_VIEW_DESC1 PromoteDesc(
      const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
            UINT                              Plane);
    
    static HRESULT NormalizeDesc(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc);
    
    static UINT GetPlaneSlice(
      const D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc);

  private:
    
    ID3D11Resource*                   m_resource;
    D3D11_RENDER_TARGET_VIEW_DESC1    m_desc;
    D3D11_VK_VIEW_INFO                m_info;
    Rc<DxvkImageView>                 m_view;
    D3D10RenderTargetView             m_d3d10;
    
  };
  
}
