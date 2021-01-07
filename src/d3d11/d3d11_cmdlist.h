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
    
    void EmitToCsThread(
            DxvkCsThread*       CsThread);
    
  private:
    
    UINT         const m_contextFlags;
    
    std::vector<DxvkCsChunkRef>         m_chunks;
    std::vector<Com<D3D11Query, false>> m_queries;

    std::atomic<bool> m_submitted = { false };
    std::atomic<bool> m_warned    = { false };

    void MarkSubmitted();
    
  };
  
}
