#pragma once

#include "dxgi_include.h"

namespace dxvk {
  class DxgiAdapter;
  class DxvkAdapter;
  class DxvkBuffer;
  class DxvkDevice;
  class DxvkImage;
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
  
  virtual dxvk::Rc<dxvk::DxvkAdapter> GetDXVKAdapter() = 0;
};


/**
 * \brief Private DXGI device interface
 * 
 * The implementation of \c IDXGIDevice stores a
 * \ref DxvkDevice which can be retrieved using
 * this interface.
 */
MIDL_INTERFACE("7a622cf6-627a-46b2-b52f-360ef3da831c")
IDXGIDevicePrivate : public IDXGIDevice {
  static const GUID guid;
  
  virtual void SetDeviceLayer(
          IUnknown* layer) = 0;
  
  virtual dxvk::Rc<dxvk::DxvkDevice> GetDXVKDevice() = 0;
};


/**
 * \brief Private buffer resource interface
 * Provides access to a raw DXVK buffer.
 */
MIDL_INTERFACE("5679becd-8547-4d93-96a1-e61a1ce7ef37")
IDXGIBufferResourcePrivate : public IDXGIResource {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkBuffer> GetDXVKBuffer() = 0;
  
  virtual void SetResourceLayer(
          IUnknown* pLayer) = 0;
};


/**
 * \brief Private image resource interface
 * Provides access to a raw DXVK image.
 */
MIDL_INTERFACE("1cfe6592-7de0-4a07-8dcb-4543cbbc6a7d")
IDXGIImageResourcePrivate : public IDXGIResource {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
  
  virtual void SetResourceLayer(
          IUnknown* pLayer) = 0;
};


DXVK_DEFINE_GUID(IDXGIAdapterPrivate);
DXVK_DEFINE_GUID(IDXGIDevicePrivate);
DXVK_DEFINE_GUID(IDXGIBufferResourcePrivate);
DXVK_DEFINE_GUID(IDXGIImageResourcePrivate);
