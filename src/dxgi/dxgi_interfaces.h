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
   * \brief Format information
   */
  enum class DxgiFormatFlag {
    Typeless = 0,
  };
  
  using DxgiFormatFlags = Flags<DxgiFormatFlag>;
  
  /**
   * \brief Format info
   * 
   * Stores a Vulkan image format for a given
   * DXGI format and some additional information
   * on how resources with the particular format
   * are supposed to be used.
   */
  struct DxgiFormatInfo {
    VkFormat            format;
    VkImageAspectFlags  aspect;
    VkComponentMapping  swizzle;
    DxgiFormatFlags     flags;
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
  virtual dxvk::DxgiFormatInfo STDMETHODCALLTYPE LookupFormat(
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
 * \brief Swap chain back buffer interface
 * 
 * Allows the swap chain and presenter to query
 * the underlying image while it is embedded in
 * a texture object specified by the client API.
 */
MIDL_INTERFACE("5679becd-8547-4d93-96a1-e61a1ce7ef37")
IDXGIPresentBackBuffer : public IUnknown {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
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
   * \brief Creates a swap chain back buffer
   * 
   * \returns \c S_OK on success
   */
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainBackBuffer(
    const DXGI_SWAP_CHAIN_DESC*       pSwapChainDesc,
          IDXGIPresentBackBuffer**    ppBackBuffer) = 0;
  
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

#ifdef _MSC_VER
struct __declspec(uuid("907bf281-ea3c-43b4-a8e4-9f231107b4ff")) IDXGIAdapterPrivate;
struct __declspec(uuid("7a622cf6-627a-46b2-b52f-360ef3da831c")) IDXGIDevicePrivate;
struct __declspec(uuid("5679becd-8547-4d93-96a1-e61a1ce7ef37")) IDXGIPresentBackBuffer;
struct __declspec(uuid("79352328-16f2-4f81-9746-9c2e2ccd43cf")) IDXGIPresentDevicePrivate;
#else
DXVK_DEFINE_GUID(IDXGIAdapterPrivate);
DXVK_DEFINE_GUID(IDXGIDevicePrivate);
DXVK_DEFINE_GUID(IDXGIPresentBackBuffer);
DXVK_DEFINE_GUID(IDXGIPresentDevicePrivate);
#endif