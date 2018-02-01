#include "d3d11_device.h"
#include "d3d11_query.h"

namespace dxvk {
  
  D3D11Query::D3D11Query(
          D3D11Device*      device,
    const D3D11_QUERY_DESC& desc)
  : m_device(device), m_desc(desc) {
    switch (desc.Query) {
      // Other query types are currently unsupported
      case D3D11_QUERY_EVENT:
      case D3D11_QUERY_OCCLUSION:
      case D3D11_QUERY_TIMESTAMP:
      case D3D11_QUERY_TIMESTAMP_DISJOINT:
      case D3D11_QUERY_OCCLUSION_PREDICATE:
        break;

      default:
        static bool errorShown = false;
        if (!std::exchange(errorShown, true))
          Logger::warn(str::format("D3D11Query: Unsupported query type ", desc.Query));
    }
  }
  
  
  D3D11Query::~D3D11Query() {
    
  }
  
    
  HRESULT STDMETHODCALLTYPE D3D11Query::QueryInterface(REFIID  riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Asynchronous);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Query);
    
    Logger::warn("D3D11Query: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11Query::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_device);
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
  
    
  void STDMETHODCALLTYPE D3D11Query::GetDesc(D3D11_QUERY_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Query::GetData(
          void*                             pData,
          UINT                              GetDataFlags) {
    static bool errorShown = false;
    static UINT64 fakeTimestamp = 0;
    
    if (!std::exchange(errorShown, true))
      Logger::warn("D3D11Query::GetData: Stub");
    
    if (pData == nullptr)
      return S_OK;
    
    switch (m_desc.Query) {
      case D3D11_QUERY_EVENT:
        *static_cast<BOOL*>(pData) = TRUE;
        return S_OK;

      case D3D11_QUERY_OCCLUSION:
        *static_cast<UINT64*>(pData) = 1;
        return S_OK;

      case D3D11_QUERY_TIMESTAMP:
        *static_cast<UINT64*>(pData) = fakeTimestamp++;
        return S_OK;

      case D3D11_QUERY_TIMESTAMP_DISJOINT:
        static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT*>(pData)->Frequency = 1000;
        static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT*>(pData)->Disjoint = FALSE;
        return S_OK;

      case D3D11_QUERY_OCCLUSION_PREDICATE:
        *static_cast<BOOL*>(pData) = TRUE;
        return S_OK;
        
      default: return E_INVALIDARG;
    }
  }
  
}
