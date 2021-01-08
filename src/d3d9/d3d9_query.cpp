#include "d3d9_query.h"

#include "d3d9_device.h"

namespace dxvk {

  D3D9Query::D3D9Query(
        D3D9DeviceEx*      pDevice,
        D3DQUERYTYPE       QueryType)
    : D3D9DeviceChild<IDirect3DQuery9>(pDevice)
    , m_queryType                          (QueryType)
    , m_state                              (D3D9_VK_QUERY_INITIAL) {
    Rc<DxvkDevice> dxvkDevice = m_parent->GetDXVKDevice();

    switch (m_queryType) {
      case D3DQUERYTYPE_VCACHE:
        break;

      case D3DQUERYTYPE_EVENT:
        m_event[0] = dxvkDevice->createGpuEvent();
        break;

      case D3DQUERYTYPE_OCCLUSION:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_OCCLUSION,
          VK_QUERY_CONTROL_PRECISE_BIT, 0);
        break;

      case D3DQUERYTYPE_TIMESTAMP:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        break;

      case D3DQUERYTYPE_TIMESTAMPDISJOINT:
        for (uint32_t i = 0; i < 2; i++) {
          m_query[i] = dxvkDevice->createGpuQuery(
            VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        }
        break;

      case D3DQUERYTYPE_TIMESTAMPFREQ:
        break;

      case D3DQUERYTYPE_VERTEXSTATS:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_PIPELINE_STATISTICS, 0, 0);
        break;

      default:
        throw DxvkError(str::format("D3D9Query: Unsupported query type ", m_queryType));
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9Query::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DQuery9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Query::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DQUERYTYPE STDMETHODCALLTYPE D3D9Query::GetType() {
    return m_queryType;
  }


  DWORD STDMETHODCALLTYPE D3D9Query::GetDataSize() {
    switch (m_queryType) {
      case D3DQUERYTYPE_VCACHE:               return sizeof(D3DDEVINFO_VCACHE);
      case D3DQUERYTYPE_RESOURCEMANAGER:      return sizeof(D3DDEVINFO_RESOURCEMANAGER);
      case D3DQUERYTYPE_VERTEXSTATS:          return sizeof(D3DDEVINFO_D3DVERTEXSTATS);
      case D3DQUERYTYPE_EVENT:                return sizeof(BOOL);
      case D3DQUERYTYPE_OCCLUSION:            return sizeof(DWORD);
      case D3DQUERYTYPE_TIMESTAMP:            return sizeof(UINT64);
      case D3DQUERYTYPE_TIMESTAMPDISJOINT:    return sizeof(BOOL);
      case D3DQUERYTYPE_TIMESTAMPFREQ:        return sizeof(UINT64);
      case D3DQUERYTYPE_PIPELINETIMINGS:      return sizeof(D3DDEVINFO_D3D9PIPELINETIMINGS);
      case D3DQUERYTYPE_INTERFACETIMINGS:     return sizeof(D3DDEVINFO_D3D9INTERFACETIMINGS);
      case D3DQUERYTYPE_VERTEXTIMINGS:        return sizeof(D3DDEVINFO_D3D9STAGETIMINGS);
      case D3DQUERYTYPE_PIXELTIMINGS:         return sizeof(D3DDEVINFO_D3D9PIPELINETIMINGS);
      case D3DQUERYTYPE_BANDWIDTHTIMINGS:     return sizeof(D3DDEVINFO_D3D9BANDWIDTHTIMINGS);
      case D3DQUERYTYPE_CACHEUTILIZATION:     return sizeof(D3DDEVINFO_D3D9CACHEUTILIZATION);
      default:                                return 0;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D9Query::Issue(DWORD dwIssueFlags) {
    // Note: No need to submit to CS if we don't do anything!

    if (dwIssueFlags == D3DISSUE_BEGIN) {
      if (QueryBeginnable(m_queryType)) {
        if (m_state == D3D9_VK_QUERY_BEGUN && QueryEndable(m_queryType))
          m_parent->End(this);

        m_parent->Begin(this);

        m_state = D3D9_VK_QUERY_BEGUN;
      }
    }
    else {
      if (QueryEndable(m_queryType)) {
        if (m_state != D3D9_VK_QUERY_BEGUN && QueryBeginnable(m_queryType))
          m_parent->Begin(this);

        m_resetCtr.fetch_add(1, std::memory_order_acquire);

        m_parent->End(this);

      }
      m_state = D3D9_VK_QUERY_ENDED;
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Query::GetData(void* pData, DWORD dwSize, DWORD dwGetDataFlags) {
    if (m_state == D3D9_VK_QUERY_CACHED) {
      // Query data was already retrieved once.
      // Use cached query data to prevent having to check the VK event
      // and having to iterate over the VK queries again
      if (likely(pData && dwSize)) {
        if (m_queryType != D3DQUERYTYPE_EVENT) {
          memcpy(pData, &m_dataCache, dwSize);
        } else {
          *static_cast<bool*>(pData) = true;
        }
      }
      return D3D_OK;
    }

    HRESULT hr = this->GetQueryData(pData, dwSize);

    bool flush = dwGetDataFlags & D3DGETDATA_FLUSH;

    // If we get S_FALSE and it's not from the fact
    // they didn't call end, do some flushy stuff...
    if (flush && hr == S_FALSE && m_state != D3D9_VK_QUERY_BEGUN) {
      this->NotifyStall();
      m_parent->FlushImplicit(FALSE);
    }

    return hr;
  }


  HRESULT D3D9Query::GetQueryData(void* pData, DWORD dwSize) {
    // Let the game know that calling end might be a good idea...
    if (m_state == D3D9_VK_QUERY_BEGUN)
      return S_FALSE;

    if (unlikely(!pData && dwSize))
      return D3DERR_INVALIDCALL;

    // The game forgot to even issue the query!
    // Let's do it for them...
    // This will issue both the begin, and the end.
    if (m_state == D3D9_VK_QUERY_INITIAL)
      this->Issue(D3DISSUE_END);

    if (m_resetCtr != 0u)
      return S_FALSE;

    if (m_queryType == D3DQUERYTYPE_EVENT) {
      DxvkGpuEventStatus status = m_event[0]->test();

      if (status == DxvkGpuEventStatus::Invalid)
        return D3DERR_INVALIDCALL;

      bool signaled = status == DxvkGpuEventStatus::Signaled;

      if (pData != nullptr)
        *static_cast<BOOL*>(pData) = signaled;

      if (signaled) {
        m_state = D3D9_VK_QUERY_CACHED;
        return D3D_OK;
      } else {
        return S_FALSE;
      }
    }
    else {
      std::array<DxvkQueryData, MaxGpuQueries> queryData = { };

      for (uint32_t i = 0; i < MaxGpuQueries && m_query[i] != nullptr; i++) {
        DxvkGpuQueryStatus status = m_query[i]->getData(queryData[i]);

        if (status == DxvkGpuQueryStatus::Invalid
         || status == DxvkGpuQueryStatus::Failed)
          return D3DERR_INVALIDCALL;

        if (status == DxvkGpuQueryStatus::Pending)
          return S_FALSE;
      }

      if (pData == nullptr)
        return D3D_OK;


      switch (m_queryType) {
        case D3DQUERYTYPE_VCACHE:
          // Don't know what the hell any of this means.
          // Nor do I care. This just makes games work.
          m_dataCache.VCache.Pattern     = MAKEFOURCC('H', 'C', 'A', 'C');
          m_dataCache.VCache.OptMethod   = 1;
          m_dataCache.VCache.CacheSize   = 24;
          m_dataCache.VCache.MagicNumber = 20;
          break;

        case D3DQUERYTYPE_OCCLUSION:
          m_dataCache.Occlusion = DWORD(queryData[0].occlusion.samplesPassed);
          break;

        case D3DQUERYTYPE_TIMESTAMP:
          m_dataCache.Timestamp = queryData[0].timestamp.time;
          break;

        case D3DQUERYTYPE_TIMESTAMPDISJOINT:
          m_dataCache.TimestampDisjoint = queryData[0].timestamp.time < queryData[1].timestamp.time;
          break;

        case D3DQUERYTYPE_TIMESTAMPFREQ:
          m_dataCache.TimestampFreq = GetTimestampQueryFrequency();
          break;

        case D3DQUERYTYPE_VERTEXSTATS:
          m_dataCache.VertexStats.NumRenderedTriangles      = queryData[0].statistic.iaPrimitives;
          m_dataCache.VertexStats.NumExtraClippingTriangles = queryData[0].statistic.clipPrimitives;
          break;

        default:
          break;
      }

      if (likely(pData && dwSize))
        memcpy(pData, &m_dataCache, dwSize);

      m_state = D3D9_VK_QUERY_CACHED;
      return D3D_OK;
    }
  }


  UINT64 D3D9Query::GetTimestampQueryFrequency() const {
    Rc<DxvkDevice>  device  = m_parent->GetDXVKDevice();
    Rc<DxvkAdapter> adapter = device->adapter();

    VkPhysicalDeviceLimits limits = adapter->deviceProperties().limits;
    return uint64_t(1'000'000'000.0f / limits.timestampPeriod);
  }


  void D3D9Query::Begin(DxvkContext* ctx) {
    switch (m_queryType) {
      case D3DQUERYTYPE_OCCLUSION:
      case D3DQUERYTYPE_VERTEXSTATS:
        ctx->beginQuery(m_query[0]);
        break;

      case D3DQUERYTYPE_TIMESTAMPDISJOINT:
        ctx->writeTimestamp(m_query[1]);
        break;

      default: break;
    }
  }


  void D3D9Query::End(DxvkContext* ctx) {
    switch (m_queryType) {
      case D3DQUERYTYPE_TIMESTAMP:
      case D3DQUERYTYPE_TIMESTAMPDISJOINT:
        ctx->writeTimestamp(m_query[0]);
        break;

      case D3DQUERYTYPE_VERTEXSTATS:
      case D3DQUERYTYPE_OCCLUSION:
        ctx->endQuery(m_query[0]);
        break;

      case D3DQUERYTYPE_EVENT:
        ctx->signalGpuEvent(m_event[0]);
        break;

      default: break;
    }

    m_resetCtr.fetch_sub(1, std::memory_order_release);
  }


  bool D3D9Query::QueryBeginnable(D3DQUERYTYPE QueryType) {
    return QueryType == D3DQUERYTYPE_OCCLUSION
        || QueryType == D3DQUERYTYPE_VERTEXSTATS
        || QueryType == D3DQUERYTYPE_TIMESTAMPDISJOINT;
  }


  bool D3D9Query::QueryEndable(D3DQUERYTYPE QueryType) {
    return QueryBeginnable(QueryType)
        || QueryType == D3DQUERYTYPE_TIMESTAMP
        || QueryType == D3DQUERYTYPE_EVENT;
  }


  HRESULT D3D9Query::QuerySupported(D3D9DeviceEx* pDevice, D3DQUERYTYPE QueryType) {
    switch (QueryType) {
      case D3DQUERYTYPE_VCACHE:
        if (!pDevice->GetOptions()->supportVCache)
          return D3DERR_NOTAVAILABLE;

        return D3D_OK;
      case D3DQUERYTYPE_EVENT:
      case D3DQUERYTYPE_OCCLUSION:
      case D3DQUERYTYPE_TIMESTAMP:
      case D3DQUERYTYPE_TIMESTAMPDISJOINT:
      case D3DQUERYTYPE_TIMESTAMPFREQ:
      case D3DQUERYTYPE_VERTEXSTATS:
        return D3D_OK;

      default:
        return D3DERR_NOTAVAILABLE;
    }
  }

}
