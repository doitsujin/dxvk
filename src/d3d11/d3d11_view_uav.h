#pragma once

#include "../dxvk/dxvk_device.h"

#include "d3d11_device_child.h"
#include "d3d11_view.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Unordered access view
   * 
   * Unordered access views are special in that they can
   * have counters, which can be used inside shaders to
   * atomically append or consume structures.
   */
  class D3D11UnorderedAccessView : public D3D11DeviceChild<ID3D11UnorderedAccessView1> {
    
  public:
    
    D3D11UnorderedAccessView(
            D3D11Device*                       pDevice,
            ID3D11Resource*                    pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc);
    
    ~D3D11UnorderedAccessView();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final;
    
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) final;
    
    void STDMETHODCALLTYPE GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) final;

    void STDMETHODCALLTYPE GetDesc1(D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc) final;
    
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
    
    Rc<DxvkBufferView> GetBufferView() const {
      return m_bufferView;
    }
    
    Rc<DxvkImageView> GetImageView() const {
      return m_imageView;
    }
    
    DxvkBufferSlice GetCounterSlice() const {
      return m_counterBuffer != nullptr
        ? DxvkBufferSlice(m_counterBuffer)
        : DxvkBufferSlice();
    }
    
    static HRESULT GetDescFromResource(
            ID3D11Resource*                    pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc);
    
    static D3D11_UNORDERED_ACCESS_VIEW_DESC1 PromoteDesc(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC*  pDesc,
            UINT                               Plane);
    
    static HRESULT NormalizeDesc(
            ID3D11Resource*                    pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc);
    
    static UINT GetPlaneSlice(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc);

  private:
    
    ID3D11Resource*                   m_resource;
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 m_desc;
    D3D11_VK_VIEW_INFO                m_info;
    Rc<DxvkBufferView>                m_bufferView;
    Rc<DxvkImageView>                 m_imageView;
    Rc<DxvkBuffer>                    m_counterBuffer;

    Rc<DxvkBuffer> CreateCounterBuffer();
    
  };
  
}
