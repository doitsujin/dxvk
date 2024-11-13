#pragma once

#include "d3d9_common_buffer.h"
#include "d3d9_common_texture.h"

namespace dxvk {

    class D3D9DeviceEx;

  /**
   * \brief Resource initialization context
   * 
   * Manages a context which is used for resource
   * initialization. This includes 
   * zero-initialization for buffers and images.
   */
  class D3D9Initializer {
    constexpr static size_t MaxTransferMemory    = 32 * 1024 * 1024;
    constexpr static size_t MaxTransferCommands  = 512;
  public:

    D3D9Initializer(
      D3D9DeviceEx*           pParent);

    ~D3D9Initializer();

    void FlushCsChunk() {
      std::lock_guard<dxvk::mutex> lock(m_csMutex);

    if (!m_csChunk->empty())
        FlushCsChunkLocked();
    }

    void NotifyContextFlush();

    void InitBuffer(
            D3D9CommonBuffer*  pBuffer);

    void InitTexture(
            D3D9CommonTexture* pTexture,
            void*              pInitialData = nullptr);

  private:

    dxvk::mutex       m_mutex;

    D3D9DeviceEx*     m_parent;
    Rc<DxvkDevice>    m_device;

    size_t            m_transferCommands  = 0;

    dxvk::mutex       m_csMutex;
    DxvkCsChunkRef    m_csChunk;

    void InitDeviceLocalBuffer(
            DxvkBufferSlice    Slice);

    void InitHostVisibleBuffer(
            DxvkBufferSlice    Slice);

    void InitDeviceLocalTexture(
            D3D9CommonTexture* pTexture);

    void InitHostVisibleTexture(
            D3D9CommonTexture* pTexture,
            void*              pInitialData,
            void*              mapPtr);

    void ThrottleAllocationLocked();

    void ExecuteFlush();

    void ExecuteFlushLocked();

    void SyncSharedTexture(
            D3D9CommonTexture* pResource);

    void FlushCsChunkLocked();

    void NotifyContextFlushLocked();

    template<typename Cmd>
    void EmitCs(Cmd&& command) {
      std::lock_guard<dxvk::mutex> lock(m_csMutex);

      if (unlikely(!m_csChunk->push(command))) {
        FlushCsChunkLocked();

        m_csChunk->push(command);
      }
    }

  };

}
