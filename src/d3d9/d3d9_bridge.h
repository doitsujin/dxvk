#pragma once

#include <windows.h>
#include "../util/config/config.h"

/**
 * The D3D9 bridge allows D3D8 to access DXVK internals.
 * For Vulkan interop without needing DXVK internals, see d3d9_interop.h.
 *
 * NOTE: You must include "d3d9_include.h" or "d3d8_include.h" before this header.
 */

/**
 * \brief D3D9 device interface for D3D8 interop
 */
MIDL_INTERFACE("D3D9D3D8-42A9-4C1E-AA97-BEEFCAFE2000")
IDxvkD3D8Bridge : public IUnknown {

  // D3D8 keeps D3D9 objects contained in a namespace.
  #ifdef DXVK_D3D9_NAMESPACE
    using IDirect3DSurface9 = d3d9::IDirect3DSurface9;
  #endif

  /**
   * \brief Updates a D3D9 surface from a D3D9 buffer
   *
   * \param [in] pDestSurface Destination surface (typically in VRAM)
   * \param [in] pSrcSurface  Source surface (typically in system memory)
   * \param [in] pSrcRect     Source rectangle
   * \param [in] pDestPoint   Destination (top-left) point
   */
  virtual HRESULT UpdateTextureFromBuffer(
      IDirect3DSurface9*        pDestSurface,
      IDirect3DSurface9*        pSrcSurface,
      const RECT*               pSrcRect,
      const POINT*              pDestPoint) = 0;
};

/**
 * \brief D3D9 instance interface for D3D8 interop
 */
MIDL_INTERFACE("D3D9D3D8-A407-773E-18E9-CAFEBEEF3000")
IDxvkD3D8InterfaceBridge : public IUnknown {
  /**
   * \brief Enforces D3D8-specific features and validations
   */
  virtual void EnableD3D8CompatibilityMode() = 0;

  /**
   * \brief Retrieves the DXVK configuration
   *
   * \returns The DXVK Config object
   */
  virtual const dxvk::Config* GetConfig() const = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(IDxvkD3D8Bridge, 0xD3D9D3D8, 0x42A9, 0x4C1E, 0xAA, 0x97, 0xBE, 0xEF, 0xCA, 0xFE, 0x20, 0x00);
__CRT_UUID_DECL(IDxvkD3D8InterfaceBridge, 0xD3D9D3D8, 0xA407, 0x773E, 0x18, 0xE9, 0xCA, 0xFE, 0xBE, 0xEF, 0x30, 0x00);
#endif

namespace dxvk {

  class D3D9DeviceEx;
  class D3D9InterfaceEx;

  class DxvkD3D8Bridge : public IDxvkD3D8Bridge {

  public:

    DxvkD3D8Bridge(D3D9DeviceEx* pDevice);

    ~DxvkD3D8Bridge();

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject);

    HRESULT UpdateTextureFromBuffer(
        IDirect3DSurface9*        pDestSurface,
        IDirect3DSurface9*        pSrcSurface,
        const RECT*               pSrcRect,
        const POINT*              pDestPoint);

  private:

    D3D9DeviceEx* m_device;

  };

  class DxvkD3D8InterfaceBridge : public IDxvkD3D8InterfaceBridge {

  public:

    DxvkD3D8InterfaceBridge(D3D9InterfaceEx* pObject);

    ~DxvkD3D8InterfaceBridge();

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject);

    void EnableD3D8CompatibilityMode();

    const Config* GetConfig() const;

  protected:

    D3D9InterfaceEx* m_interface;

  };

}
