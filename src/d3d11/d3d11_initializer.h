#pragma once

#include "d3d11_buffer.h"
#include "d3d11_texture.h"

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
    constexpr static size_t MaxTransferMemory    = 32 * 1024 * 1024;
    constexpr static size_t MaxTransferCommands  = 512;
  public:

    D3D11Initializer(
            D3D11Device*                pParent);
    
    ~D3D11Initializer();

    void Flush();

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