#pragma once

#include "dxgi_include.h"

namespace dxvk {
  class DxgiAdapter;
  class DxvkAdapter;
  class DxvkDevice;
}
  
/**
 * \brief DXVK adapter
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
 * \brief DXVK device
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


template<> inline GUID const& __mingw_uuidof<IDXGIAdapterPrivate>() { return IDXGIAdapterPrivate::guid; }
template<> inline GUID const& __mingw_uuidof<IDXGIDevicePrivate> () { return IDXGIDevicePrivate ::guid; }
