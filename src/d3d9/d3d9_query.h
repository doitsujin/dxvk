#pragma once

#include "d3d9_device_child.h"

#include "../dxvk/dxvk_context.h"

namespace dxvk {

  enum D3D9_VK_QUERY_STATE : uint32_t {
    D3D9_VK_QUERY_INITIAL,
    D3D9_VK_QUERY_BEGUN,
    D3D9_VK_QUERY_ENDED,
    D3D9_VK_QUERY_CACHED
  };

  union D3D9_QUERY_DATA {
    D3DDEVINFO_VCACHE         VCache;
    DWORD                     Occlusion;
    UINT64                    Timestamp;
    BOOL                      TimestampDisjoint;
    UINT64                    TimestampFreq;
    D3DDEVINFO_D3DVERTEXSTATS VertexStats;
  };

  class D3D9Query : public D3D9DeviceChild<IDirect3DQuery9> {
    constexpr static uint32_t MaxGpuQueries = 2;
    constexpr static uint32_t MaxGpuEvents  = 1;
  public:

    D3D9Query(
            D3D9DeviceEx*      pDevice,
            D3DQUERYTYPE       QueryType);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DQUERYTYPE STDMETHODCALLTYPE GetType() final;

    DWORD STDMETHODCALLTYPE GetDataSize() final;

    HRESULT STDMETHODCALLTYPE Issue(DWORD dwIssueFlags) final;

    HRESULT STDMETHODCALLTYPE GetData(void* pData, DWORD dwSize, DWORD dwGetDataFlags) final;

    HRESULT GetQueryData(void* pData, DWORD dwSize);

    void Begin(DxvkContext* ctx);
    void End(DxvkContext* ctx);

    static bool QueryBeginnable(D3DQUERYTYPE QueryType);
    static bool QueryEndable(D3DQUERYTYPE QueryType);

    static HRESULT QuerySupported(D3D9DeviceEx* pDevice, D3DQUERYTYPE QueryType);

    bool IsEvent() const {
      return m_queryType == D3DQUERYTYPE_EVENT;
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

  private:

    D3DQUERYTYPE      m_queryType;

    D3D9_VK_QUERY_STATE m_state;

    std::array<Rc<DxvkQuery>, MaxGpuQueries> m_query;
    std::array<Rc<DxvkEvent>, MaxGpuEvents>  m_event;

    uint32_t m_stallMask = 0;
    bool     m_stallFlag = false;

    std::atomic<uint32_t> m_resetCtr = { 0u };

    D3D9_QUERY_DATA m_dataCache;

    UINT64 GetTimestampQueryFrequency() const;

  };

}