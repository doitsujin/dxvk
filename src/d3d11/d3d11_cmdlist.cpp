#include "d3d11_cmdlist.h"
#include "d3d11_device.h"

namespace dxvk {
    
  D3D11CommandList::D3D11CommandList(
          D3D11Device*  pDevice,
          UINT          ContextFlags)
  : m_device      (pDevice),
    m_contextFlags(ContextFlags) { }
  
  
  D3D11CommandList::~D3D11CommandList() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11CommandList::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11CommandList);
    
    Logger::warn("D3D11CommandList::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11CommandList::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  UINT STDMETHODCALLTYPE D3D11CommandList::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void D3D11CommandList::AddChunk(Rc<DxvkCsChunk>&& Chunk) {
    m_chunks.push_back(std::move(Chunk));
  }
  
  
  void D3D11CommandList::EmitToCommandList(ID3D11CommandList* pCommandList) {
    auto cmdList = static_cast<D3D11CommandList*>(pCommandList);
    
    for (auto chunk : m_chunks)
      cmdList->m_chunks.push_back(chunk);
  }
  
  
  void D3D11CommandList::EmitToCsThread(DxvkCsThread* CsThread) {
    for (auto chunk : m_chunks)
      CsThread->dispatchChunk(Rc<DxvkCsChunk>(chunk));
  }
  
}