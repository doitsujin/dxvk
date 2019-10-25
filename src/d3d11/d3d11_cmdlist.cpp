#include "d3d11_cmdlist.h"
#include "d3d11_device.h"

namespace dxvk {
    
  D3D11CommandList::D3D11CommandList(
          D3D11Device*               pDevice,
          D3D11CommandListAllocator* pAllocator)
  : m_device(pDevice), m_allocator(pAllocator) {

  }
  
  
  D3D11CommandList::~D3D11CommandList() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11CommandList::AddRef() {
    ULONG refCount = m_refCount++;
    if (!refCount)
      m_device->AddRef();
    return refCount + 1;
  }
  

  ULONG STDMETHODCALLTYPE D3D11CommandList::Release() {
    ULONG refCount = --m_refCount;

    if (!refCount) {
      Reset();

      m_allocator->RecycleCommandList(this);
      m_device->Release();
    }

    return refCount;
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
  
  
  void STDMETHODCALLTYPE D3D11CommandList::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  UINT STDMETHODCALLTYPE D3D11CommandList::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void D3D11CommandList::AddChunk(DxvkCsChunkRef&& Chunk) {
    m_chunks.push_back(std::move(Chunk));
  }
  
  
  void D3D11CommandList::EmitToCommandList(ID3D11CommandList* pCommandList) {
    auto cmdList = static_cast<D3D11CommandList*>(pCommandList);
    
    for (const auto& chunk : m_chunks)
      cmdList->m_chunks.push_back(chunk);
    
    MarkSubmitted();
  }
  
  
  void D3D11CommandList::EmitToCsThread(DxvkCsThread* CsThread) {
    for (const auto& chunk : m_chunks)
      CsThread->dispatchChunk(DxvkCsChunkRef(chunk));
    
    MarkSubmitted();
  }


  void D3D11CommandList::Reset() {
    m_chunks.clear();

    m_contextFlags  = 0;
    m_submitted     = false;
    m_warned        = false;
  }
  
  
  void D3D11CommandList::MarkSubmitted() {
    if (m_submitted.exchange(true) && !m_warned.exchange(true)
     && m_device->GetOptions()->dcSingleUseMode) {
      Logger::warn(
        "D3D11: Command list submitted multiple times,\n"
        "       but d3d11.dcSingleUseMode is enabled");
    }
  }
  

  D3D11CommandListAllocator::D3D11CommandListAllocator(D3D11Device* pDevice)
  : m_device(pDevice) {

  }


  D3D11CommandListAllocator::~D3D11CommandListAllocator() {
    for (uint32_t i = 0; i < m_listCount; i++)
      delete m_lists[i];
  }


  D3D11CommandList* D3D11CommandListAllocator::AllocCommandList(
          UINT                  ContextFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    D3D11CommandList* result = nullptr;
    
    if (m_listCount)
      result = m_lists[--m_listCount];
    else
      result = new D3D11CommandList(m_device, this);

    result->SetContextFlags(ContextFlags);
    return result;
  }


  void D3D11CommandListAllocator::RecycleCommandList(
          D3D11CommandList*     pCommandList) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_listCount < m_lists.size())
      m_lists[m_listCount++] = pCommandList;
    else
      delete pCommandList;
  }

}