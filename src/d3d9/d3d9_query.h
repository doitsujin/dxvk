#pragma once

#include "d3d9_device_child.h"

namespace dxvk {

  class D3D9Query : public Direct3DDeviceChild9<IDirect3DQuery9> {

  public:

    D3D9Query(
            Direct3DDevice9Ex* pDevice,
            D3DQUERYTYPE       QueryType);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DQUERYTYPE STDMETHODCALLTYPE GetType() final;

    DWORD STDMETHODCALLTYPE GetDataSize() final;

    HRESULT STDMETHODCALLTYPE Issue(DWORD dwIssueFlags) final;

    HRESULT STDMETHODCALLTYPE GetData(void* pData, DWORD dwSize, DWORD dwGetDataFlags) final;

    void Begin(DxvkContext* ctx);
    void End(DxvkContext* ctx);

  private:

    D3DQUERYTYPE      m_queryType;

    Rc<DxvkGpuQuery>  m_query;
    Rc<DxvkGpuEvent>  m_event;

    UINT64 GetTimestampQueryFrequency() const;

  };

}