#include "d3d9_query.h"

namespace dxvk {

  D3D9Query::D3D9Query(
        Direct3DDevice9Ex* pDevice,
        D3DQUERYTYPE       QueryType)
    : Direct3DDeviceChild9<IDirect3DQuery9>(pDevice)
    , m_queryType                          (QueryType) {
    Rc<DxvkDevice> dxvkDevice = m_parent->GetDXVKDevice();

    switch (m_queryType) {
      case D3DQUERYTYPE_VCACHE:
        break;

      case D3DQUERYTYPE_EVENT:
        m_event = dxvkDevice->createGpuEvent();
        break;

      case D3DQUERYTYPE_OCCLUSION:
        m_query = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_OCCLUSION,
          VK_QUERY_CONTROL_PRECISE_BIT, 0);
        break;

      case D3DQUERYTYPE_TIMESTAMP:
        m_query = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        break;

      case D3DQUERYTYPE_TIMESTAMPDISJOINT:
        break;

      case D3DQUERYTYPE_TIMESTAMPFREQ:
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
    dwIssueFlags & D3DISSUE_END
      ? m_parent->End(this)
      : m_parent->Begin(this);
      
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9Query::GetData(void* pData, DWORD dwSize, DWORD dwGetDataFlags) {
    m_parent->SynchronizeCsThread();

    if (m_queryType == D3DQUERYTYPE_EVENT) {
      DxvkGpuEventStatus status = m_event->test();

      if (status == DxvkGpuEventStatus::Invalid)
        return D3DERR_INVALIDCALL;

      bool signaled = status == DxvkGpuEventStatus::Signaled;

      if (pData != nullptr)
        * static_cast<BOOL*>(pData) = signaled;

      return signaled ? D3D_OK : S_FALSE;
    }
    else {
      DxvkQueryData queryData = {};

      if (m_query != nullptr) {
        DxvkGpuQueryStatus status = m_query->getData(queryData);

        if (status == DxvkGpuQueryStatus::Invalid
          || status == DxvkGpuQueryStatus::Failed)
          return D3DERR_INVALIDCALL;

        if (status == DxvkGpuQueryStatus::Pending) {
          m_parent->FlushImplicit(FALSE);
          return S_FALSE;
        }
      }

      if (pData == nullptr)
        return D3D_OK;

      switch (m_queryType) {
        case D3DQUERYTYPE_VCACHE: {
          // Don't know what the hell any of this means.
          // Nor do I care. This just makes games work.
          auto* data = static_cast<D3DDEVINFO_VCACHE*>(pData);
          data->Pattern     = MAKEFOURCC('H', 'C', 'A', 'C');
          data->OptMethod   = 1;
          data->CacheSize   = 16;
          data->MagicNumber = 8;
          return D3D_OK;
        }

        case D3DQUERYTYPE_OCCLUSION:
          *static_cast<DWORD*>(pData) = DWORD(queryData.occlusion.samplesPassed);
          return D3D_OK;

        case D3DQUERYTYPE_TIMESTAMP:
          *static_cast<UINT64*>(pData) = queryData.timestamp.time;
          return D3D_OK;

        case D3DQUERYTYPE_TIMESTAMPDISJOINT:
          *static_cast<BOOL*>(pData) = FALSE;
          return D3D_OK;

        case D3DQUERYTYPE_TIMESTAMPFREQ:
          *static_cast<UINT64*>(pData) = GetTimestampQueryFrequency();
          return D3D_OK;

        default:
          return D3D_OK;
      }
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
      case D3DQUERYTYPE_TIMESTAMP:
      case D3DQUERYTYPE_OCCLUSION:
        ctx->beginQuery(m_query);
        break;

      default: break;
    }
  }

  void D3D9Query::End(DxvkContext* ctx) {
    switch (m_queryType) {
      case D3DQUERYTYPE_TIMESTAMP:
        ctx->writeTimestamp(m_query);
        break;

      case D3DQUERYTYPE_OCCLUSION:
        ctx->endQuery(m_query);
        break;

      case D3DQUERYTYPE_EVENT:
        ctx->signalGpuEvent(m_event);
        break;

      default: break;
    }
  }

}