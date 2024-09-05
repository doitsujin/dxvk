#pragma once

#include "d3d11_buffer.h"
#include "d3d11_texture.h"

namespace dxvk {

  class D3D11Device;

  using D3D11InitChunkDispatchProc = std::function<void (DxvkCsChunkRef&&)>;

  /**
   * \brief Resource initialization context
   * 
   * Manages a context which is used for resource
   * initialization. This includes initialization
   * with application-defined data, as well as
   * zero-initialization for buffers and images.
   */
  class D3D11Initializer {
    constexpr static size_t MaxTransferMemory    = 32 * 1024 * 1024;
    constexpr static size_t MaxTransferCommands  = 512;

    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;
  public:

    D3D11Initializer(
            D3D11Device*                pParent);
    
    ~D3D11Initializer();

    void EmitToCsThread(const D3D11InitChunkDispatchProc& DispatchProc);

    void InitBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
    void InitTexture(
            D3D11CommonTexture*         pTexture,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitUavCounter(
            D3D11UnorderedAccessView*   pUav);

  private:

    dxvk::mutex       m_mutex;

    D3D11Device*      m_parent;
    Rc<DxvkDevice>    m_device;

    size_t            m_transferCommands  = 0;
    size_t            m_transferMemory    = 0;

    DxvkStagingBuffer           m_staging;
    DxvkCsChunkRef              m_chunk;
    std::vector<DxvkCsChunkRef> m_chunks;
    std::vector<D3D11CommonTexture*> m_texturesUpdateSeqNum;

    void InitDeviceLocalBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitHostVisibleBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitDeviceLocalTexture(
            D3D11CommonTexture*         pTexture,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitHostVisibleTexture(
            D3D11CommonTexture*         pTexture,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitTiledTexture(
            D3D11CommonTexture*         pTexture);

    void SyncKeyedMutex(ID3D11Resource *pResource);

    DxvkCsChunkRef AllocCsChunk();

    template<typename Cmd>
    void EmitCs(Cmd&& command) {
      if (unlikely(!m_chunk->push(command))) {
        m_chunks.push_back(std::move(m_chunk));
        m_chunk = AllocCsChunk();
        m_chunk->push(command);
      }
    }

  };

}
