#pragma once

#include "d3d11_buffer.h"
#include "d3d11_texture.h"

namespace dxvk {

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
  public:

    D3D11Initializer(
      const Rc<DxvkDevice>&             Device);
    
    ~D3D11Initializer();

    void Flush();

    void InitBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
    void InitTexture(
            D3D11CommonTexture*         pTexture,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
  private:

    std::mutex        m_mutex;

    Rc<DxvkDevice>    m_device;
    Rc<DxvkContext>   m_context;

    size_t            m_transferCommands  = 0;
    size_t            m_transferMemory    = 0;

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
    
    void FlushImplicit();
    void FlushInternal();

  };

}