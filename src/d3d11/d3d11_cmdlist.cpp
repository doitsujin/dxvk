#include "d3d11_cmdlist.h"
#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_texture.h"

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
    
    if (logQueryInterfaceError(__uuidof(ID3D11CommandList), riid)) {
      Logger::warn("D3D11CommandList::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11CommandList::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void D3D11CommandList::AddQuery(D3D11Query* pQuery) {
    m_queries.emplace_back(pQuery);
  }


  uint64_t D3D11CommandList::AddChunk(DxvkCsChunkRef&& Chunk) {
    m_chunks.push_back(std::move(Chunk));
    return m_chunks.size() - 1;
  }
  
  
  uint64_t D3D11CommandList::AddCommandList(
          D3D11CommandList*   pCommandList) {
    // This will be the chunk ID of the first chunk
    // added, for the purpose of resource tracking.
    uint64_t baseChunkId = m_chunks.size();
    
    for (const auto& chunk : pCommandList->m_chunks)
      m_chunks.push_back(chunk);

    for (const auto& query : pCommandList->m_queries)
      m_queries.push_back(query);

    for (const auto& resource : pCommandList->m_resources) {
      TrackedResource entry = resource;
      entry.chunkId += baseChunkId;

      m_resources.push_back(std::move(entry));
    }

    // Return ID of the last chunk added. The command list
    // added can never be empty, so do not handle zero.
    return m_chunks.size() - 1;
  }


  void D3D11CommandList::EmitToCsThread(
    const D3D11ChunkDispatchProc& DispatchProc) {
    for (const auto& query : m_queries)
      query->DoDeferredEnd();

    for (size_t i = 0, j = 0; i < m_chunks.size(); i++) {
      // If there are resources to track for the current chunk,
      // use a strong flush hint to dispatch GPU work quickly.
      GpuFlushType flushType = GpuFlushType::ImplicitWeakHint;

      if (j < m_resources.size() && m_resources[j].chunkId == i)
        flushType = GpuFlushType::ImplicitStrongHint;

      // Dispatch the chunk and capture its sequence number
      uint64_t seq = DispatchProc(DxvkCsChunkRef(m_chunks[i]), flushType);

      // Track resource sequence numbers for the added chunk
      while (j < m_resources.size() && m_resources[j].chunkId == i)
        TrackResourceSequenceNumber(m_resources[j++].ref, seq);
    }
  }
  
  
  void D3D11CommandList::TrackResourceUsage(
          ID3D11Resource*     pResource,
          D3D11_RESOURCE_DIMENSION ResourceType,
          UINT                Subresource,
          uint64_t            ChunkId) {
    TrackedResource entry;
    entry.ref = D3D11ResourceRef(pResource, Subresource, ResourceType);
    entry.chunkId = ChunkId;

    m_resources.push_back(std::move(entry));
  }


  void D3D11CommandList::TrackResourceSequenceNumber(
    const D3D11ResourceRef&   Resource,
          uint64_t            Seq) {
    ID3D11Resource* iface = Resource.Get();

    switch (Resource.GetType()) {
      case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        break;

      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        auto impl = static_cast<D3D11Buffer*>(iface);
        impl->TrackSequenceNumber(Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        auto impl = static_cast<D3D11Texture1D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        auto impl = static_cast<D3D11Texture2D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        auto impl = static_cast<D3D11Texture3D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;
    }
  }
  
}
