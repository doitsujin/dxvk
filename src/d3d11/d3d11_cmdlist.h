#pragma once

#include <functional>

#include "d3d11_context.h"

namespace dxvk {
  
  using D3D11ChunkDispatchProc = std::function<uint64_t (DxvkCsChunkRef&&, GpuFlushType)>;

  class D3D11CommandList : public D3D11DeviceChild<ID3D11CommandList> {
    
  public:
    
    D3D11CommandList(
            D3D11Device*  pDevice,
            UINT          ContextFlags);
    
    ~D3D11CommandList();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    UINT STDMETHODCALLTYPE GetContextFlags() final;
    
    void AddQuery(
            D3D11Query*         pQuery);
    
    uint64_t AddChunk(
            DxvkCsChunkRef&&    Chunk);

    uint64_t AddCommandList(
            D3D11CommandList*   pCommandList);

    void EmitToCsThread(
      const D3D11ChunkDispatchProc& DispatchProc);

    void TrackResourceUsage(
            ID3D11Resource*     pResource,
            D3D11_RESOURCE_DIMENSION ResourceType,
            UINT                Subresource,
            uint64_t            ChunkId);

  private:

    struct TrackedResource {
      D3D11ResourceRef  ref;
      uint64_t          chunkId;
    };

    UINT m_contextFlags;
    
    std::vector<DxvkCsChunkRef>         m_chunks;
    std::vector<Com<D3D11Query, false>> m_queries;
    std::vector<TrackedResource>        m_resources;

    std::atomic<bool> m_submitted = { false };
    std::atomic<bool> m_warned    = { false };

    void TrackResourceSequenceNumber(
      const D3D11ResourceRef&   Resource,
            uint64_t            Seq);

    void MarkSubmitted();
    
  };
  
}
