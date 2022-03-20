#pragma once

#include "d3d11_context.h"

namespace dxvk {
  
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
    
    void AddChunk(
            DxvkCsChunkRef&&    Chunk);

    void AddQuery(
            D3D11Query*         pQuery);
    
    void EmitToCommandList(
            ID3D11CommandList*  pCommandList);
    
    uint64_t EmitToCsThread(
            DxvkCsThread*       CsThread);

    void TrackResourceUsage(
            ID3D11Resource*     pResource,
            D3D11_RESOURCE_DIMENSION ResourceType,
            UINT                Subresource);

  private:

    UINT         const m_contextFlags;
    
    std::vector<DxvkCsChunkRef>         m_chunks;
    std::vector<Com<D3D11Query, false>> m_queries;
    std::vector<D3D11ResourceRef>       m_resources;

    std::atomic<bool> m_submitted = { false };
    std::atomic<bool> m_warned    = { false };

    void TrackResourceSequenceNumber(
      const D3D11ResourceRef&   Resource,
            uint64_t            Seq);

    void MarkSubmitted();
    
  };
  
}
