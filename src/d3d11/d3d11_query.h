#pragma once

#include "../dxvk/dxvk_gpu_event.h"
#include "../dxvk/dxvk_gpu_query.h"

#include "../d3d10/d3d10_query.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Query : public D3D11DeviceChild<ID3D11Predicate> {
    
  public:
    
    D3D11Query(
            D3D11Device*      device,
      const D3D11_QUERY_DESC& desc);
    
    ~D3D11Query();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    UINT STDMETHODCALLTYPE GetDataSize();
    
    void STDMETHODCALLTYPE GetDesc(
            D3D11_QUERY_DESC *pDesc) final;
    
    void Begin(DxvkContext* ctx);
    
    void End(DxvkContext* ctx);
    
    HRESULT STDMETHODCALLTYPE GetData(
            void*                             pData,
            UINT                              GetDataFlags);
    
    D3D10Query* GetD3D10Iface() {
      return &m_d3d10;
    }
    
  private:
    
    D3D11Device* const m_device;
    D3D11_QUERY_DESC   m_desc;
    
    Rc<DxvkGpuQuery>  m_query = nullptr;
    Rc<DxvkGpuEvent>  m_event = nullptr;
    
    uint32_t m_revision = 0;

    D3D10Query m_d3d10;

    UINT64 GetTimestampQueryFrequency() const;
    
  };
  
}
