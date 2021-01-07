#pragma once

#include "../dxvk/dxvk_gpu_event.h"
#include "../dxvk/dxvk_gpu_query.h"

#include "../d3d10/d3d10_query.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  enum D3D11_VK_QUERY_STATE : uint32_t {
    D3D11_VK_QUERY_INITIAL,
    D3D11_VK_QUERY_BEGUN,
    D3D11_VK_QUERY_ENDED,
  };
  
  class D3D11Query : public D3D11DeviceChild<ID3D11Query1> {
    constexpr static uint32_t MaxGpuQueries = 2;
    constexpr static uint32_t MaxGpuEvents  = 1;
  public:
    
    D3D11Query(
            D3D11Device*        device,
      const D3D11_QUERY_DESC1&  desc);
    
    ~D3D11Query();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    UINT STDMETHODCALLTYPE GetDataSize();
    
    void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC* pDesc) final;

    void STDMETHODCALLTYPE GetDesc1(D3D11_QUERY_DESC1* pDesc) final;

    void Begin(DxvkContext* ctx);
    
    void End(DxvkContext* ctx);
    
    bool STDMETHODCALLTYPE DoBegin();

    bool STDMETHODCALLTYPE DoEnd();

    HRESULT STDMETHODCALLTYPE GetData(
            void*                             pData,
            UINT                              GetDataFlags);
    
    void DoDeferredEnd() {
      m_state = D3D11_VK_QUERY_ENDED;
      m_resetCtr.fetch_add(1, std::memory_order_acquire);
    }

    bool IsScoped() const {
      return m_desc.Query != D3D11_QUERY_EVENT
          && m_desc.Query != D3D11_QUERY_TIMESTAMP;
    }

    bool IsEvent() const {
      return m_desc.Query == D3D11_QUERY_EVENT;
    }

    bool IsStalling() const {
      return m_stallFlag;
    }

    void NotifyEnd() {
      m_stallMask <<= 1;
    }

    void NotifyStall() {
      m_stallMask |= 1;
      m_stallFlag |= bit::popcnt(m_stallMask) >= 16;
    }
    
    D3D10Query* GetD3D10Iface() {
      return &m_d3d10;
    }

    static HRESULT ValidateDesc(const D3D11_QUERY_DESC1* pDesc);

    static ID3D11Predicate* AsPredicate(ID3D11Query* pQuery) {
      // ID3D11Predicate and ID3D11Query have the same vtable. This
      // saves us some headache in all query-related functions.
      return static_cast<ID3D11Predicate*>(pQuery);
    }
    
    static D3D11Query* FromPredicate(ID3D11Predicate* pPredicate) {
      return static_cast<D3D11Query*>(static_cast<ID3D11Query*>(pPredicate));
    }
    
  private:
    
    D3D11_QUERY_DESC1  m_desc;

    D3D11_VK_QUERY_STATE m_state;
    
    std::array<Rc<DxvkGpuQuery>, MaxGpuQueries> m_query;
    std::array<Rc<DxvkGpuEvent>, MaxGpuEvents>  m_event;

    D3D10Query m_d3d10;

    uint32_t m_stallMask = 0;
    bool     m_stallFlag = false;

    std::atomic<uint32_t> m_resetCtr = { 0u };

    UINT64 GetTimestampQueryFrequency() const;
    
  };
  
}
