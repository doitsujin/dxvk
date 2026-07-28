#pragma once

#include <windows.h>
#include "../util/config/config.h"
#include "../util/util_flags.h"

enum class DxvkD3DCompatibility : uint8_t {
  D3D8,
};

/**
 * The D3D9 bridge allows D3D8 to access DXVK internals.
 * For Vulkan interop without needing DXVK internals, see d3d9_interop.h.
 *
 * NOTE: You must include "d3d9_include.h" or "d3d8_include.h" before this header.
 */

/**
 * \brief D3D9 device interface for D3D8 interop
 */
MIDL_INTERFACE("D3D0D3D9-42A9-4C1E-AA97-BEEFCAFE2000")
IDxvkLegacyD3DDeviceBridge : public IUnknown {

  // D3D8 keeps D3D9 objects contained in a namespace.
  #ifdef DXVK_D3D9_NAMESPACE
    using IDirect3DSurface9 = d3d9::IDirect3DSurface9;
    using D3DFORMAT = d3d9::D3DFORMAT;
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

  /**
   * \brief Checks if a particular surface format is supported by D3D9
   *
   * \param [in] Format D3DFORMAT value to be checked
   */
  virtual bool IsSupportedSurfaceFormat(D3DFORMAT Format) = 0;
};

/**
 * \brief D3D9 instance interface for D3D8 interop
 */
MIDL_INTERFACE("D3D0D3D9-A407-773E-18E9-CAFEBEEF3000")
IDxvkLegacyD3DInterfaceBridge : public IUnknown {
  /**
   * \brief Enforces legacy D3D features and validations
   *
   * \param [in] d3dCompatibility D3D compatibility level to be set
   */
  virtual void SetD3DCompatibility(DxvkD3DCompatibility d3dCompatibility) const = 0;

  /**
   * \brief Retrieves the DXVK configuration
   *
   * \returns The DXVK Config object
   */
  virtual const dxvk::Config* GetConfig() const = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(IDxvkLegacyD3DDeviceBridge,    0xD3D0D3D9, 0x42A9, 0x4C1E, 0xAA, 0x97, 0xBE, 0xEF, 0xCA, 0xFE, 0x20, 0x00);
__CRT_UUID_DECL(IDxvkLegacyD3DInterfaceBridge, 0xD3D0D3D9, 0xA407, 0x773E, 0x18, 0xE9, 0xCA, 0xFE, 0xBE, 0xEF, 0x30, 0x00);
#endif

namespace dxvk {

  using D3DCompatibility = DxvkD3DCompatibility;
  using D3DCompatibilityFlags = Flags<D3DCompatibility>;

  class D3D9DeviceEx;
  class D3D9InterfaceEx;

  class DxvkLegacyD3DDeviceBridge : public IDxvkLegacyD3DDeviceBridge {

  public:

    DxvkLegacyD3DDeviceBridge(D3D9DeviceEx* pDevice);

    ~DxvkLegacyD3DDeviceBridge();

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

    bool IsSupportedSurfaceFormat(D3DFORMAT Format);

  private:

    D3D9DeviceEx* m_device;

  };

  class DxvkLegacyD3DInterfaceBridge : public IDxvkLegacyD3DInterfaceBridge {

  public:

    DxvkLegacyD3DInterfaceBridge(D3D9InterfaceEx* pObject);

    ~DxvkLegacyD3DInterfaceBridge();

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject);

    void SetD3DCompatibility(D3DCompatibility d3dCompatibility) const;

    const Config* GetConfig() const;

  protected:

    D3D9InterfaceEx* m_interface;

  };

}
