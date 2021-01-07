#include "d3d11_cmdlist.h"
#include "d3d11_device.h"

namespace dxvk {
    
  D3D11CommandList::D3D11CommandList(
          D3D11Device*  pDevice,
          UINT          ContextFlags)
  : D3D11DeviceChild<ID3D11CommandList>(pDevice),
    m_contextFlags(ContextFlags) { }
  
  
  D3D11CommandList::~D3D11CommandList() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11CommandList::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11CommandList)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11CommandList::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11CommandList::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void D3D11CommandList::AddChunk(DxvkCsChunkRef&& Chunk) {
    m_chunks.push_back(std::move(Chunk));
  }
  
  
  void D3D11CommandList::AddQuery(D3D11Query* pQuery) {
    m_queries.emplace_back(pQuery);
  }


  void D3D11CommandList::EmitToCommandList(ID3D11CommandList* pCommandList) {
    auto cmdList = static_cast<D3D11CommandList*>(pCommandList);
    
    for (const auto& chunk : m_chunks)
      cmdList->m_chunks.push_back(chunk);

    for (const auto& query : m_queries)
      cmdList->m_queries.push_back(query);

    MarkSubmitted();
  }
  
  
  void D3D11CommandList::EmitToCsThread(DxvkCsThread* CsThread) {
    for (const auto& query : m_queries)
      query->DoDeferredEnd();

    for (const auto& chunk : m_chunks)
      CsThread->dispatchChunk(DxvkCsChunkRef(chunk));
    
    MarkSubmitted();
  }
  
  
  void D3D11CommandList::MarkSubmitted() {
    if (m_submitted.exchange(true) && !m_warned.exchange(true)
     && m_parent->GetOptions()->dcSingleUseMode) {
      Logger::warn(
        "D3D11: Command list submitted multiple times,\n"
        "       but d3d11.dcSingleUseMode is enabled");
    }
  }
  
}
