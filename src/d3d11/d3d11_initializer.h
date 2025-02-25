#pragma once

#include "../dxvk/dxvk_staging.h"

#include "d3d11_buffer.h"
#include "d3d11_shader.h"
#include "d3d11_texture.h"
#include "d3d11_view_uav.h"

namespace dxvk {

  class D3D11Device;

  /**
   * \brief Resource initialization context
   * 
   * Manages a context which is used for resource
   * initialization. This includes initialization
   * with application-defined data, as well as
   * zero-initialization for buffers and images.
   */
  class D3D11Initializer {
    // Use a staging buffer with a linear allocator to service small uploads
    constexpr static VkDeviceSize StagingBufferSize = 1ull << 20;
  public:

    // Maximum number of copy and clear commands to record before flushing
    constexpr static size_t MaxCommandsPerSubmission = 512u;

    // Maximum amount of staging memory to allocate before flushing
    constexpr static size_t MaxMemoryPerSubmission = (env::is32BitHostPlatform() ? 12u : 48u) << 20;

    // Maximum amount of memory in flight. If there are pending uploads while
    // this limit is exceeded, further initialization will be stalled.
    constexpr static size_t MaxMemoryInFlight = 3u * MaxMemoryPerSubmission;

    D3D11Initializer(
            D3D11Device*                pParent);
    
    ~D3D11Initializer();

    void FlushCsChunk() {
      std::lock_guard<dxvk::mutex> lock(m_csMutex);

      if (!m_csChunk->empty())
        FlushCsChunkLocked();
    }

    void NotifyContextFlush();

    void InitBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
    void InitTexture(
            D3D11CommonTexture*         pTexture,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);

    void InitUavCounter(
            D3D11UnorderedAccessView*   pUav);
    
    void InitShaderIcb(
            D3D11CommonShader*          pShader,
            size_t                      IcbSize,
      const void*                       pIcbData);

  private:

    dxvk::mutex       m_mutex;

    D3D11Device*      m_parent;
    Rc<DxvkDevice>    m_device;
    
    DxvkStagingBuffer m_stagingBuffer;
    Rc<sync::Fence>   m_stagingSignal;

    size_t            m_transferCommands  = 0;

    dxvk::mutex       m_csMutex;
    DxvkCsChunkRef    m_csChunk;

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

    void ThrottleAllocationLocked();

    void ExecuteFlush();

    void ExecuteFlushLocked();

    void SyncSharedTexture(
            D3D11CommonTexture*         pResource);

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
