#pragma once

#include "d3d11_include.h"

#include "../dxgi/dxgi_interfaces.h"

#include "../dxvk/dxvk_device.h"

/**
 * \brief Private device interface
 * 
 * This interface is used by \ref DxgiSwapChain
 * in order to communicate with the device.
 */
MIDL_INTERFACE("ab2a2a58-d2ac-4ca0-9ad9-a260cafa00c8")
ID3D11DevicePrivate : public ID3D11Device {
  static const GUID guid;
  
  /**
   * \brief Wraps swap chain image into a texture interface
   * 
   * Creates an interface to the back buffer image of a
   * swap chain. This interface will be returned by the
   * swap chain's \c GetBuffer method.
   * \param [in] image Image to wrap
   * \param [in] pSwapChainDesc Swap chain properties
   * \param [in] ppInterface Target interface
   * \returns \c S_OK on success
   */
  virtual HRESULT WrapSwapChainBackBuffer(
          IDXGIImageResourcePrivate*  pResource,
    const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
          IUnknown**                  ppInterface) = 0;
  
  /**
   * \brief Flushes the immediate context
   * 
   * Used by the swap chain's \c Present method to
   * ensure that all rendering commands get dispatched
   * before presenting the swap chain's back buffer.
   * \returns \c S_OK on success
   */
  virtual HRESULT FlushRenderingCommands() = 0;
  
  /**
   * \brief Underlying DXVK device
   * \returns DXVK device handle
   */
  virtual dxvk::Rc<dxvk::DxvkDevice> GetDXVKDevice() = 0;
};


DXVK_DEFINE_GUID(ID3D11DevicePrivate);