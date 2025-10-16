#include "ddraw_common_interface.h"

#include "d3d_common_device.h"
#include "d3d_common_texture.h"

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"

#include "d3d3/d3d3_interface.h"

#include <algorithm>

namespace dxvk {

  std::atomic<D3DTEXTUREHANDLE> DDrawCommonInterface::s_textureHandle = 0;

  DDrawCommonInterface::DDrawCommonInterface(const D3DOptions& d3dOptions)
    : m_d3dOptions ( d3dOptions ) {
  }

  DDrawCommonInterface::~DDrawCommonInterface() {
  }

  D3D3Interface* DDrawCommonInterface::GetOrCreateD3D3Interface() {
    if (likely(m_d3d3Intf != nullptr))
      return m_d3d3Intf;

    // In case a D3D3 interface doesn't exist, query one from the
    // base DDraw interface, where it will also be cached
    if (likely(m_intf != nullptr)) {
      HRESULT hr = m_intf->QueryInterface(__uuidof(IDirect3D), reinterpret_cast<void**>(&m_d3d3Intf));
      if (unlikely(FAILED(hr)))
        return nullptr;
      // The call to QueryInterface has incremented the public ref
      m_d3d3Intf->Release();

      return m_d3d3Intf;
    }

    return nullptr;
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface* surface) {
    auto it = s_surfaces.find(surface);
    if (likely(it != s_surfaces.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface* surface) {
    s_surfaces.insert(surface);
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface* surface) {
    auto it = s_surfaces.find(surface);
    if (likely(it != s_surfaces.end())) {
      s_surfaces.erase(it);
    } else {
      Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface2* surface) {
    auto it = s_surfaces2.find(surface);
    if (likely(it != s_surfaces2.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface2* surface) {
    s_surfaces2.insert(surface);
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface2* surface) {
    auto it = s_surfaces2.find(surface);
    if (likely(it != s_surfaces2.end())) {
      s_surfaces2.erase(it);
    } else {
      Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface3* surface) {
    auto it = s_surfaces3.find(surface);
    if (likely(it != s_surfaces3.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface3* surface) {
    s_surfaces3.insert(surface);
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface3* surface) {
    auto it = s_surfaces3.find(surface);
    if (likely(it != s_surfaces3.end())) {
      s_surfaces3.erase(it);
    } else {
      Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface4* surface) {
    auto it = s_surfaces4.find(surface);
    if (likely(it != s_surfaces4.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface4* surface) {
    s_surfaces4.insert(surface);
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface4* surface) {
    auto it = s_surfaces4.find(surface);
    if (likely(it != s_surfaces4.end())) {
      s_surfaces4.erase(it);
    } else {
      Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface7* surface) {
    auto it = s_surfaces7.find(surface);
    if (likely(it != s_surfaces7.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface7* surface) {
    s_surfaces7.insert(surface);
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface7* surface) {
    auto it = s_surfaces7.find(surface);
    if (likely(it != s_surfaces7.end())) {
      s_surfaces7.erase(it);
    } else {
      Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
    }
  }

  DDraw4Surface* DDrawCommonInterface::GetSurface4FromTextureHandle(D3DTEXTUREHANDLE handle) {
    auto texturesIter = s_textures.find(handle);

    if (unlikely(texturesIter == s_textures.end())) {
      Logger::warn(str::format("DDrawCommonInterface::GetSurface4FromTextureHandle: Invalid handle: ", handle));
      return nullptr;
    }

    return texturesIter->second->GetDD4Surface();
  }

  DDrawSurface* DDrawCommonInterface::GetSurfaceFromTextureHandle(D3DTEXTUREHANDLE handle) {
    auto texturesIter = s_textures.find(handle);

    if (unlikely(texturesIter == s_textures.end())) {
      Logger::warn(str::format("DDrawCommonInterface::GetSurfaceFromTextureHandle: Invalid handle: ", handle));
      return nullptr;
    }

    return texturesIter->second->GetDDSurface();
  }

}