#pragma once

#include "../dxvk/dxvk_include.h"

#include "dxgi_include.h"

namespace dxvk {
  class DxgiAdapter;
  class DxvkAdapter;
  class DxvkBuffer;
  class DxvkDevice;
  class DxvkImage;
  
  /**
   * \brief Format pair
   * 
   * For a DXGI format, this stores two Vulkan formats:
   * The format that directly corresponds to the DXGI
   * format, and a similar format that the device can
   * use. If the device supports the desired format,
   * both formats will be equal.
   */
  struct DxgiFormatPair {
    VkFormat wanted = VK_FORMAT_UNDEFINED;
    VkFormat actual = VK_FORMAT_UNDEFINED;
  };
  
  /**
   * \brief Format lookup mode
   * 
   * When looking up an image format, additional information
   * might be needed on how the image is going to be used.
   * This is used to properly map typeless formats and color
   * formats to depth formats if they are used on depth images.
   */
  enum class DxgiFormatMode : uint32_t {
    Any   = 0,
    Color = 1,
    Depth = 2,
  };
}
  
/**
 * \brief Private DXGI adapter interface
 * 
 * The implementation of \c IDXGIAdapter holds a
 * \ref DxvkAdapter which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("907bf281-ea3c-43b4-a8e4-9f231107b4ff")
IDXGIAdapterPrivate : public IDXGIAdapter1 {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkAdapter> STDMETHODCALLTYPE GetDXVKAdapter() = 0;
  
  /**
   * \brief Maps a DXGI format to a compatible Vulkan format
   * 
   * For color formats, the returned Vulkan format has the
   * same memory layout as the DXGI format so that it can
   * be mapped and copied to buffers. For depth-stencil
   * formats, this is not guaranteed.
   * \param [in] format The DXGI format
   * \param [in] mode Format lookup mode
   * \returns Vulkan format pair
   */
  virtual dxvk::DxgiFormatPair STDMETHODCALLTYPE LookupFormat(
          DXGI_FORMAT          format,
          dxvk::DxgiFormatMode mode) = 0;
};


/**
 * \brief Private DXGI device interface
 * 
 * The implementation of \c IDXGIDevice stores a
 * \ref DxvkDevice which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("7a622cf6-627a-46b2-b52f-360ef3da831c")
IDXGIDevicePrivate : public IDXGIDevice1 {
  static const GUID guid;
  
  virtual void STDMETHODCALLTYPE SetDeviceLayer(
          IUnknown* layer) = 0;
  
  virtual dxvk::Rc<dxvk::DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice() = 0;
};


/**
 * \brief Private buffer resource interface
 * Provides access to a raw DXVK buffer.
 */
MIDL_INTERFACE("5679becd-8547-4d93-96a1-e61a1ce7ef37")
IDXGIBufferResourcePrivate : public IDXGIResource {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkBuffer> STDMETHODCALLTYPE GetDXVKBuffer() = 0;
  
  virtual void STDMETHODCALLTYPE SetResourceLayer(
          IUnknown* pLayer) = 0;
};


/**
 * \brief Private image resource interface
 * Provides access to a raw DXVK image.
 */
MIDL_INTERFACE("1cfe6592-7de0-4a07-8dcb-4543cbbc6a7d")
IDXGIImageResourcePrivate : public IDXGIResource {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> STDMETHODCALLTYPE GetDXVKImage() = 0;
  
  virtual void STDMETHODCALLTYPE SetResourceLayer(
          IUnknown* pLayer) = 0;
};


/**
 * \brief Private presentation device interface
 * 
 * Allows a swap chain to communicate with the device
 * in order to flush pending commands or create the
 * back buffer interface.
 */
MIDL_INTERFACE("79352328-16f2-4f81-9746-9c2e2ccd43cf")
IDXGIPresentDevicePrivate : public IUnknown {
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
  virtual HRESULT STDMETHODCALLTYPE WrapSwapChainBackBuffer(
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
  virtual HRESULT STDMETHODCALLTYPE FlushRenderingCommands() = 0;
  
  /**
   * \brief Underlying DXVK device
   * 
   * \param [in] riid Device type
   * \param [in] ppDevice device
   * \returns DXVK device handle
   */
  virtual HRESULT STDMETHODCALLTYPE GetDevice(
          REFGUID     riid,
          void**      ppDevice) = 0;
};


DXVK_DEFINE_GUID(IDXGIAdapterPrivate);
DXVK_DEFINE_GUID(IDXGIDevicePrivate);
DXVK_DEFINE_GUID(IDXGIPresentDevicePrivate);
DXVK_DEFINE_GUID(IDXGIBufferResourcePrivate);
DXVK_DEFINE_GUID(IDXGIImageResourcePrivate);
