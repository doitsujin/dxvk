#include "d3d11_device.h"
#include "d3d11_query.h"

namespace dxvk {
  
  D3D11Query::D3D11Query(
          D3D11Device*       device,
    const D3D11_QUERY_DESC1& desc)
  : D3D11DeviceChild<ID3D11Query1>(device),
    m_desc(desc),
    m_state(D3D11_VK_QUERY_INITIAL),
    m_d3d10(this) {
    Rc<DxvkDevice> dxvkDevice = m_parent->GetDXVKDevice();

    switch (m_desc.Query) {
      case D3D11_QUERY_EVENT:
        m_event[0] = dxvkDevice->createGpuEvent();
        break;
        
      case D3D11_QUERY_OCCLUSION:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_OCCLUSION,
          VK_QUERY_CONTROL_PRECISE_BIT, 0);
        break;
      
      case D3D11_QUERY_OCCLUSION_PREDICATE:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_OCCLUSION, 0, 0);
        break;
        
      case D3D11_QUERY_TIMESTAMP:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        break;
      
      case D3D11_QUERY_TIMESTAMP_DISJOINT:
        for (uint32_t i = 0; i < 2; i++) {
          m_query[i] = dxvkDevice->createGpuQuery(
            VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        }
        break;
      
      case D3D11_QUERY_PIPELINE_STATISTICS:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_PIPELINE_STATISTICS, 0, 0);
        break;
      
      case D3D11_QUERY_SO_STATISTICS:
      case D3D11_QUERY_SO_STATISTICS_STREAM0:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0:
        // FIXME it is technically incorrect to map
        // SO_OVERFLOW_PREDICATE to the first stream,
        // but this is good enough for D3D10 behaviour
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 0, 0);
        break;
      
      case D3D11_QUERY_SO_STATISTICS_STREAM1:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 0, 1);
        break;
      
      case D3D11_QUERY_SO_STATISTICS_STREAM2:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 0, 2);
        break;
      
      case D3D11_QUERY_SO_STATISTICS_STREAM3:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3:
        m_query[0] = dxvkDevice->createGpuQuery(
          VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 0, 3);
        break;
      
      default:
        throw DxvkError(str::format("D3D11: Unhandled query type: ", desc.Query));
    }
  }
  
  
  D3D11Query::~D3D11Query() {

  }
  
    
  HRESULT STDMETHODCALLTYPE D3D11Query::QueryInterface(REFIID  riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Asynchronous)
     || riid == __uuidof(ID3D11Query)
     || riid == __uuidof(ID3D11Query1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Asynchronous)
     || riid == __uuidof(ID3D10Query)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    if (m_desc.Query == D3D11_QUERY_OCCLUSION_PREDICATE) {
      if (riid == __uuidof(ID3D11Predicate)) {
        *ppvObject = AsPredicate(ref(this));
        return S_OK;
      }

      if (riid == __uuidof(ID3D10Predicate)) {
        *ppvObject = ref(&m_d3d10);
        return S_OK;
      }
    }
    
    Logger::warn("D3D11Query: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Query::GetDataSize() {
    switch (m_desc.Query) {
      case D3D11_QUERY_EVENT:
        return sizeof(BOOL);
      
      case D3D11_QUERY_OCCLUSION:
        return sizeof(UINT64);
      
      case D3D11_QUERY_TIMESTAMP:
        return sizeof(UINT64);
      
      case D3D11_QUERY_TIMESTAMP_DISJOINT:
        return sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT);
      
      case D3D11_QUERY_PIPELINE_STATISTICS:
        return sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS);
      
      case D3D11_QUERY_OCCLUSION_PREDICATE:
        return sizeof(BOOL);
      
      case D3D11_QUERY_SO_STATISTICS:
      case D3D11_QUERY_SO_STATISTICS_STREAM0:
      case D3D11_QUERY_SO_STATISTICS_STREAM1:
      case D3D11_QUERY_SO_STATISTICS_STREAM2:
      case D3D11_QUERY_SO_STATISTICS_STREAM3:
        return sizeof(D3D11_QUERY_DATA_SO_STATISTICS);
      
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2:
      case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3:
        return sizeof(BOOL);
    }
    
    Logger::err("D3D11Query: Failed to query data size");
    return 0;
  }
  
    
  void STDMETHODCALLTYPE D3D11Query::GetDesc(D3D11_QUERY_DESC* pDesc) {
    pDesc->Query     = m_desc.Query;
    pDesc->MiscFlags = m_desc.MiscFlags;
  }
  
  
  void STDMETHODCALLTYPE D3D11Query::GetDesc1(D3D11_QUERY_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  void D3D11Query::Begin(DxvkContext* ctx) {
    switch (m_desc.Query) {
      case D3D11_QUERY_EVENT:
      case D3D11_QUERY_TIMESTAMP:
        break;

      case D3D11_QUERY_TIMESTAMP_DISJOINT:
        ctx->writeTimestamp(m_query[1]);
        break;
      
      default:
        ctx->beginQuery(m_query[0]);
    }
  }
  
  
  void D3D11Query::End(DxvkContext* ctx) {
    switch (m_desc.Query) {
      case D3D11_QUERY_EVENT:
        ctx->signalGpuEvent(m_event[0]);
        break;
      
      case D3D11_QUERY_TIMESTAMP:
      case D3D11_QUERY_TIMESTAMP_DISJOINT:
        ctx->writeTimestamp(m_query[0]);
        break;
      
      default:
        ctx->endQuery(m_query[0]);
    }

    m_resetCtr.fetch_sub(1, std::memory_order_release);
  }
  
  
  bool STDMETHODCALLTYPE D3D11Query::DoBegin() {
    if (!IsScoped() || m_state == D3D11_VK_QUERY_BEGUN)
      return false;

    m_state = D3D11_VK_QUERY_BEGUN;
    return true;
  }

  bool STDMETHODCALLTYPE D3D11Query::DoEnd() {
    // Apparently the D3D11 runtime implicitly begins the query
    // if it is in the wrong state at the time End is called, so
    // let the caller react to it instead of just failing here.
    bool result = m_state == D3D11_VK_QUERY_BEGUN || !IsScoped();

    m_state = D3D11_VK_QUERY_ENDED;
    m_resetCtr.fetch_add(1, std::memory_order_acquire);
    return result;
  }


  HRESULT STDMETHODCALLTYPE D3D11Query::GetData(
          void*                             pData,
          UINT                              GetDataFlags) {
    if (m_state != D3D11_VK_QUERY_ENDED)
      return DXGI_ERROR_INVALID_CALL;

    if (m_resetCtr != 0u)
      return S_FALSE;

    if (m_desc.Query == D3D11_QUERY_EVENT) {
      DxvkGpuEventStatus status = m_event[0]->test();

      if (status == DxvkGpuEventStatus::Invalid)
        return DXGI_ERROR_INVALID_CALL;
      
      bool signaled = status == DxvkGpuEventStatus::Signaled;

      if (pData != nullptr)
        *static_cast<BOOL*>(pData) = signaled;
      
      return signaled ? S_OK : S_FALSE;
    } else {
      std::array<DxvkQueryData, MaxGpuQueries> queryData = { };
      
      for (uint32_t i = 0; i < MaxGpuQueries && m_query[i] != nullptr; i++) {
        DxvkGpuQueryStatus status = m_query[i]->getData(queryData[i]);

        if (status == DxvkGpuQueryStatus::Invalid
         || status == DxvkGpuQueryStatus::Failed)
          return DXGI_ERROR_INVALID_CALL;
        
        if (status == DxvkGpuQueryStatus::Pending)
          return S_FALSE;
      }
      
      if (pData == nullptr)
        return S_OK;
      
      switch (m_desc.Query) {
        case D3D11_QUERY_OCCLUSION:
          *static_cast<UINT64*>(pData) = queryData[0].occlusion.samplesPassed;
          return S_OK;
        
        case D3D11_QUERY_OCCLUSION_PREDICATE:
          *static_cast<BOOL*>(pData) = queryData[0].occlusion.samplesPassed != 0;
          return S_OK;
        
        case D3D11_QUERY_TIMESTAMP:
          *static_cast<UINT64*>(pData) = queryData[0].timestamp.time;
          return S_OK;
        
        case D3D11_QUERY_TIMESTAMP_DISJOINT: {
          auto data = static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT*>(pData);
          data->Frequency = GetTimestampQueryFrequency();
          data->Disjoint  = queryData[0].timestamp.time < queryData[1].timestamp.time;
        } return S_OK;
        
        case D3D11_QUERY_PIPELINE_STATISTICS: {
          auto data = static_cast<D3D11_QUERY_DATA_PIPELINE_STATISTICS*>(pData);
          data->IAVertices    = queryData[0].statistic.iaVertices;
          data->IAPrimitives  = queryData[0].statistic.iaPrimitives;
          data->VSInvocations = queryData[0].statistic.vsInvocations;
          data->GSInvocations = queryData[0].statistic.gsInvocations;
          data->GSPrimitives  = queryData[0].statistic.gsPrimitives;
          data->CInvocations  = queryData[0].statistic.clipInvocations;
          data->CPrimitives   = queryData[0].statistic.clipPrimitives;
          data->PSInvocations = queryData[0].statistic.fsInvocations;
          data->HSInvocations = queryData[0].statistic.tcsPatches;
          data->DSInvocations = queryData[0].statistic.tesInvocations;
          data->CSInvocations = queryData[0].statistic.csInvocations;
        } return S_OK;

        case D3D11_QUERY_SO_STATISTICS:
        case D3D11_QUERY_SO_STATISTICS_STREAM0:
        case D3D11_QUERY_SO_STATISTICS_STREAM1:
        case D3D11_QUERY_SO_STATISTICS_STREAM2:
        case D3D11_QUERY_SO_STATISTICS_STREAM3: {
          auto data = static_cast<D3D11_QUERY_DATA_SO_STATISTICS*>(pData);
          data->NumPrimitivesWritten    = queryData[0].xfbStream.primitivesWritten;
          data->PrimitivesStorageNeeded = queryData[0].xfbStream.primitivesNeeded;
        } return S_OK;
          
        case D3D11_QUERY_SO_OVERFLOW_PREDICATE:
        case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0:
        case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1:
        case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2:
        case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3: {
          auto data = static_cast<BOOL*>(pData);
          *data = queryData[0].xfbStream.primitivesNeeded
                > queryData[0].xfbStream.primitivesWritten;
        } return S_OK;

        default:
          Logger::err(str::format("D3D11: Unhandled query type in GetData: ", m_desc.Query));
          return E_INVALIDARG;
      }
    }
  }
  
  
  UINT64 D3D11Query::GetTimestampQueryFrequency() const {
    Rc<DxvkDevice>  device  = m_parent->GetDXVKDevice();
    Rc<DxvkAdapter> adapter = device->adapter();

    VkPhysicalDeviceLimits limits = adapter->deviceProperties().limits;
    return uint64_t(1'000'000'000.0f / limits.timestampPeriod);
  }


  HRESULT D3D11Query::ValidateDesc(const D3D11_QUERY_DESC1* pDesc) {
    if (pDesc->Query       >= D3D11_QUERY_PIPELINE_STATISTICS
     && pDesc->ContextType >  D3D11_CONTEXT_TYPE_3D)
      return E_INVALIDARG;
    
    return S_OK;
  }
  
}
