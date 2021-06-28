#pragma once

#include "d3d9_common_buffer.h"
#include "d3d9_common_texture.h"

namespace dxvk {

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
      const Rc<DxvkDevice>&             Device);
    
    ~D3D9Initializer();

    void Flush();

    void InitBuffer(
            D3D9CommonBuffer*  pBuffer);
    
    void InitTexture(
            D3D9CommonTexture* pTexture,
            void*              pInitialData = nullptr);
    
  private:

    dxvk::mutex       m_mutex;

    Rc<DxvkDevice>    m_device;
    Rc<DxvkContext>   m_context;

    size_t            m_transferCommands  = 0;
    size_t            m_transferMemory    = 0;

    void InitDeviceLocalBuffer(
            DxvkBufferSlice    Slice);

    void InitHostVisibleBuffer(
            DxvkBufferSlice    Slice);

    void InitDeviceLocalTexture(
            D3D9CommonTexture* pTexture);

    void InitHostVisibleTexture(
            D3D9CommonTexture* pTexture,
            void*              pInitialData);
    
    void FlushImplicit();
    void FlushInternal();

  };

}