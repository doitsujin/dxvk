#pragma once

#include "d3d11_device_child.h"

#include "../dxvk/dxvk_event.h"
#include "../dxvk/dxvk_query.h"

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
    
    uint32_t Reset();
    
    bool HasBeginEnabled() const;
    
    void Begin(DxvkContext* ctx, uint32_t revision);
    
    void End(DxvkContext* ctx);
    
    void Signal(DxvkContext* ctx, uint32_t revision);
    
    HRESULT STDMETHODCALLTYPE GetData(
            void*                             pData,
            UINT                              GetDataFlags);
    
  private:
    
    D3D11Device* const m_device;
    D3D11_QUERY_DESC   m_desc;
    
    Rc<DxvkQuery> m_query = nullptr;
    Rc<DxvkEvent> m_event = nullptr;
    
    uint32_t m_revision = 0;

    UINT64 GetTimestampQueryFrequency() const;
    
  };
  
}
