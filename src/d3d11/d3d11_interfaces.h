#pragma once

#include "d3d11_include.h"

#include "../dxgi/dxgi_interfaces.h"

#include "../dxvk/dxvk_device.h"

MIDL_INTERFACE("776fc4de-9cd9-4a4d-936a-7837d20ec5d9")
ID3D11BufferPrivate : public ID3D11Buffer {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkBuffer> GetDXVKBuffer() = 0;
};


MIDL_INTERFACE("cc62022f-eb7c-473c-b58c-c621bc27b405")
ID3D11Texture1DPrivate : public ID3D11Texture1D {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
};


MIDL_INTERFACE("0ca9d5af-96e6-41f2-a2c0-6b43d4dc837d")
ID3D11Texture2DPrivate : public ID3D11Texture2D {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
};


MIDL_INTERFACE("b0f7d56e-761e-46c0-8fca-d465b742b2f8")
ID3D11Texture3DPrivate : public ID3D11Texture3D {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImage> GetDXVKImage() = 0;
};


MIDL_INTERFACE("175a9e94-115c-416a-967d-afabadfa0ea8")
ID3D11RenderTargetViewPrivate : public ID3D11RenderTargetView {
  static const GUID guid;
  
  virtual dxvk::Rc<dxvk::DxvkImageView> GetDXVKImageView() = 0;
};


DXVK_DEFINE_GUID(ID3D11BufferPrivate);
DXVK_DEFINE_GUID(ID3D11Texture1DPrivate);
DXVK_DEFINE_GUID(ID3D11Texture2DPrivate);
DXVK_DEFINE_GUID(ID3D11Texture3DPrivate);
DXVK_DEFINE_GUID(ID3D11RenderTargetViewPrivate);