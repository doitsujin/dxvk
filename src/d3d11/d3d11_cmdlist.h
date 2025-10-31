#pragma once

#include <functional>

#include "d3d11_context.h"

namespace dxvk {
  
  using D3D11ChunkDispatchProc = std::function<uint64_t (DxvkCsChunkRef&&, uint64_t, GpuFlushType)>;

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
            DxvkCsChunkRef&&    Chunk,
            uint64_t            Cost);

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

    struct ChunkEntry {
      ChunkEntry() = default;
      ChunkEntry(DxvkCsChunkRef&& c, uint64_t v)
      : chunk(std::move(c)), cost(v) { }
      DxvkCsChunkRef chunk = { };
      uint64_t cost = 0u;
    };

    struct TrackedResource {
      D3D11ResourceRef  ref;
      uint64_t          chunkId;
    };

    UINT m_contextFlags = 0u;

    std::vector<ChunkEntry>             m_chunks;
    std::vector<Com<D3D11Query, false>> m_queries;
    std::vector<TrackedResource>        m_resources;

    D3DDestructionNotifier              m_destructionNotifier;

    void TrackResourceSequenceNumber(
      const D3D11ResourceRef&   Resource,
            uint64_t            Seq);
    
  };
  
}
