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

    for (const auto& resource : m_resources)
      cmdList->m_resources.push_back(resource);

    MarkSubmitted();
  }
  
  
  uint64_t D3D11CommandList::EmitToCsThread(DxvkCsThread* CsThread) {
    uint64_t seq = 0;

    for (const auto& query : m_queries)
      query->DoDeferredEnd();

    for (const auto& chunk : m_chunks)
      seq = CsThread->dispatchChunk(DxvkCsChunkRef(chunk));
    
    for (const auto& resource : m_resources)
      TrackResourceSequenceNumber(resource, seq);

    MarkSubmitted();
    return seq;
  }
  
  
  void D3D11CommandList::TrackResourceUsage(
          ID3D11Resource*     pResource,
          D3D11_RESOURCE_DIMENSION ResourceType,
          UINT                Subresource) {
    m_resources.emplace_back(pResource, Subresource, ResourceType);
  }


  void D3D11CommandList::TrackResourceSequenceNumber(
    const D3D11ResourceRef&   Resource,
          uint64_t            Seq) {
    ID3D11Resource* iface = Resource.Get();
    UINT subresource = Resource.GetSubresource();

    switch (Resource.GetType()) {
      case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        break;

      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        auto impl = static_cast<D3D11Buffer*>(iface);
        impl->TrackSequenceNumber(Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        auto impl = static_cast<D3D11Texture1D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(subresource, Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        auto impl = static_cast<D3D11Texture2D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(subresource, Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        auto impl = static_cast<D3D11Texture3D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(subresource, Seq);
      } break;
    }
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
