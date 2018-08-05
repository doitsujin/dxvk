#pragma once

#include "../dxvk/dxvk_device.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Unordered access view
   * 
   * Unordered access views are special in that they can
   * have counters, which can be used inside shaders to
   * atomically append or consume structures.
   */
  class D3D11UnorderedAccessView : public D3D11DeviceChild<ID3D11UnorderedAccessView> {
    
  public:
    
    D3D11UnorderedAccessView(
            D3D11Device*                      pDevice,
            ID3D11Resource*                   pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
    
    ~D3D11UnorderedAccessView();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) final;
    
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) final;
    
    void STDMETHODCALLTYPE GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) final;
    
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
      return m_counterSlice;
    }
    
    static HRESULT GetDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
    
    static HRESULT NormalizeDesc(
            ID3D11Resource*                   pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
    
  private:
    
    Com<D3D11Device>                  m_device;
    ID3D11Resource*                   m_resource;
    D3D11_UNORDERED_ACCESS_VIEW_DESC  m_desc;
    Rc<DxvkBufferView>                m_bufferView;
    Rc<DxvkImageView>                 m_imageView;
    DxvkBufferSlice                   m_counterSlice;
    
  };
  
}
