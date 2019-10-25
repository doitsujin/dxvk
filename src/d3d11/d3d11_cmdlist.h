#pragma once

#include <mutex>
#include <vector>

#include "d3d11_context.h"

namespace dxvk {

  class D3D11CommandListAllocator;
  
  class D3D11CommandList : public D3D11DeviceChild<ID3D11CommandList, NoWrapper> {
    
  public:
    
    D3D11CommandList(
            D3D11Device*               pDevice,
            D3D11CommandListAllocator* pAllocator);
    
    ~D3D11CommandList();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device **ppDevice) final;
    
    UINT STDMETHODCALLTYPE GetContextFlags() final;

    void SetContextFlags(UINT ContextFlags) {
      m_contextFlags = ContextFlags;
    }
    
    void AddChunk(
            DxvkCsChunkRef&&    Chunk);
    
    void EmitToCommandList(
            ID3D11CommandList*  pCommandList);
    
    void EmitToCsThread(
            DxvkCsThread*       CsThread);
    
  private:
    
    D3D11Device*                m_device;
    D3D11CommandListAllocator*  m_allocator;

    uint32_t m_contextFlags = 0;

    std::vector<DxvkCsChunkRef> m_chunks;

    std::atomic<bool> m_submitted = { false };
    std::atomic<bool> m_warned    = { false };

    std::atomic<uint32_t> m_refCount = { 0u };

    void Reset();

    void MarkSubmitted();
    
  };
  

  /**
   * \brief Command list allocator
   *
   * Creates and recycles command list instances
   * in order to reduce deferred context overhead.
   */
  class D3D11CommandListAllocator {

  public:

    D3D11CommandListAllocator(
            D3D11Device*  pDevice);

    ~D3D11CommandListAllocator();

    /**
     * \brief Allocates a command list
     *
     * \param [in] ContextFlags Flags of the parent context
     * \returns The command list
     */
    D3D11CommandList* AllocCommandList(
            UINT                  ContextFlags);

    /**
     * \brief Recycles a command list
     *
     * Automatically called when the command list
     * in question reaches a ref count of zero.
     * \param [in] pCommandList The command list
     */
    void RecycleCommandList(
            D3D11CommandList*     pCommandList);

  private:

    D3D11Device* m_device;

    std::mutex                        m_mutex;
    std::array<D3D11CommandList*, 64> m_lists;

    uint32_t m_listCount = 0;

  };
  
}