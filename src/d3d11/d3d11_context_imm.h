#pragma once

#include "../util/util_time.h"

#include "../util/sync/sync_signal.h"

#include "d3d11_context.h"
#include "d3d11_state_object.h"
#include "d3d11_video.h"

namespace dxvk {
  
  class D3D11Buffer;
  class D3D11CommonTexture;

  class D3D11ImmediateContext : public D3D11CommonContext<D3D11ImmediateContext> {
    friend class D3D11CommonContext<D3D11ImmediateContext>;
    friend class D3D11SwapChain;
    friend class D3D11VideoContext;
    friend class D3D11DXGIKeyedMutex;
  public:
    
    D3D11ImmediateContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device);
    ~D3D11ImmediateContext();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject);

    HRESULT STDMETHODCALLTYPE GetData(
            ID3D11Asynchronous*         pAsync,
            void*                       pData,
            UINT                        DataSize,
            UINT                        GetDataFlags);
    
    void STDMETHODCALLTYPE Begin(
            ID3D11Asynchronous*         pAsync);
    
    void STDMETHODCALLTYPE End(
            ID3D11Asynchronous*         pAsync);
    
    void STDMETHODCALLTYPE Flush();
    
    void STDMETHODCALLTYPE Flush1(
            D3D11_CONTEXT_TYPE          ContextType,
            HANDLE                      hEvent);

    HRESULT STDMETHODCALLTYPE Signal(
            ID3D11Fence*                pFence,
            UINT64                      Value);
    
    HRESULT STDMETHODCALLTYPE Wait(
            ID3D11Fence*                pFence,
            UINT64                      Value);

    void STDMETHODCALLTYPE ExecuteCommandList(
            ID3D11CommandList*  pCommandList,
            BOOL                RestoreContextState);
    
    HRESULT STDMETHODCALLTYPE FinishCommandList(
            BOOL                RestoreDeferredContextState,
            ID3D11CommandList   **ppCommandList);
    
    HRESULT STDMETHODCALLTYPE Map(
            ID3D11Resource*             pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    void STDMETHODCALLTYPE Unmap(
            ID3D11Resource*             pResource,
            UINT                        Subresource);
            
    void STDMETHODCALLTYPE SwapDeviceContextState(
            ID3DDeviceContextState*           pState,
            ID3DDeviceContextState**          ppPreviousState);

    void Acquire11on12Resource(
            ID3D11Resource*             pResource,
            VkImageLayout               SrcLayout);

    void Release11on12Resource(
            ID3D11Resource*             pResource,
            VkImageLayout               DstLayout);

    void SynchronizeCsThread(
            uint64_t                          SequenceNumber);

    D3D10Multithread& GetMultithread() {
        return m_multithread;
    }

    D3D10DeviceLock LockContext() {
      return m_multithread.AcquireLock();
    }

  private:
    
    DxvkCsThread            m_csThread;
    uint64_t                m_csSeqNum = 0ull;

    uint32_t                m_mappedImageCount = 0u;

    VkDeviceSize            m_maxImplicitDiscardSize = 0ull;

    Rc<sync::CallbackFence> m_submissionFence;
    uint64_t                m_submissionId = 0ull;
    DxvkSubmitStatus        m_submitStatus;

    uint64_t                m_flushSeqNum = 0ull;
    GpuFlushTracker         m_flushTracker;

    D3D10Multithread        m_multithread;
    D3D11VideoContext       m_videoContext;

    Com<D3D11DeviceContextState, false> m_stateObject;
    
    HRESULT MapBuffer(
            D3D11Buffer*                pResource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    HRESULT MapImage(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    void UnmapImage(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource);
    
    void ReadbackImageBuffer(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource);

    void UpdateDirtyImageRegion(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource,
      const D3D11_COMMON_TEXTURE_REGION* pRegion);

    void UpdateMappedBuffer(
            D3D11Buffer*                pDstBuffer,
            UINT                        Offset,
            UINT                        Length,
      const void*                       pSrcData,
            UINT                        CopyFlags);

    void SynchronizeDevice();

    void EndFrame();
    
    bool WaitForResource(
      const Rc<DxvkResource>&           Resource,
            uint64_t                    SequenceNumber,
            D3D11_MAP                   MapType,
            UINT                        MapFlags);
    
    void EmitCsChunk(DxvkCsChunkRef&& chunk);

    void TrackTextureSequenceNumber(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource);

    void TrackBufferSequenceNumber(
            D3D11Buffer*                pResource);

    uint64_t GetCurrentSequenceNumber();

    uint64_t GetPendingCsChunks();

    void ConsiderFlush(
            GpuFlushType                FlushType);

    void ExecuteFlush(
            GpuFlushType                FlushType,
            HANDLE                      hEvent,
            BOOL                        Synchronize);

  };
  
}
