#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class DDrawCommonSurface;
  class DDrawCommonInterface;
  class D3DCommonInterface;

  class DDraw7Surface;
  class DDraw4Surface;
  class DDrawSurface;

  class D3D7Device;
  class D3D6Device;
  class D3D5Device;
  class D3D3Device;

  class D3DCommonDevice : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonDevice(
          DDrawCommonInterface* commonIntf,
          GUID deviceGUID,
          const d3d9::D3DPRESENT_PARAMETERS* pParams9,
          DWORD creationFlags9);

    ~D3DCommonDevice();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    D3DCommonInterface* GetCommonD3DInterface() const;

    HRESULT ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params);

    DDrawSurface* GetCurrentRenderTarget() const;

    DDraw4Surface* GetCurrentRenderTarget4() const;

    DDraw7Surface* GetCurrentRenderTarget7() const;

    bool IsCurrentRenderTarget(DDrawCommonSurface* commonSurface) const;

    void SetInScene(bool inScene) {
      m_inScene = inScene;
    }

    bool IsInScene() const {
      return m_inScene;
    }

    GUID GetDeviceGUID() const {
      return m_deviceGUID;
    }

    bool IsHALOrTNLHALDevice() const {
      return m_deviceGUID == IID_IDirect3DHALDevice ||
             // Functionally identical to a HAL device
             m_deviceGUID == IID_WineD3DDevice ||
             m_deviceGUID == IID_IDirect3DTnLHalDevice;
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    void SetTotalTextureMemory(uint32_t totalMemory) {
      m_totalMemory = totalMemory;
    }

    uint32_t GetTotalTextureMemory() const {
      return m_totalMemory;
    }

    D3DMATERIALHANDLE GetCurrentMaterialHandle() const {
      return m_materialHandle;
    }

    void SetCurrentMaterialHandle(D3DMATERIALHANDLE materialHandle) {
      m_materialHandle = materialHandle;
    }

    D3DTEXTUREHANDLE GetCurrentTextureHandle() const {
      return m_textureHandle;
    }

    void SetCurrentTextureHandle(D3DTEXTUREHANDLE textureHandle) {
      m_textureHandle = textureHandle;
    }

    DWORD GetColorKeyEnable() const {
      return m_colorKeyEnable;
    }

    void SetColorKeyEnable(DWORD colorKeyEnable) {
      m_colorKeyEnable = colorKeyEnable;
    }

    DWORD GetColorKeyBlendEnable() const {
      return m_colorKeyBlendEnable;
    }

    void SetColorKeyBlendEnable(DWORD colorKeyBlendEnable) {
      m_colorKeyBlendEnable = colorKeyBlendEnable;
    }

    DWORD GetAntialias() const {
      return m_antialias;
    }

    void SetAntialias(DWORD antialias) {
      m_antialias = antialias;
    }

    D3DLINEPATTERN GetLinePattern() const {
      return m_linePattern;
    }

    void SetLinePattern(D3DLINEPATTERN linePattern) {
      m_linePattern = linePattern;
    }

    DWORD GetTextureMapBlend() const {
      return m_textureMapBlend;
    }

    void SetTextureMapBlend(DWORD textureMapBlend) {
      m_textureMapBlend = textureMapBlend;
    }

    const d3d9::D3DPRESENT_PARAMETERS* GetPresentParameters() const {
      return &m_params9;
    }

    d3d9::D3DMULTISAMPLE_TYPE GetMultiSampleType() const {
      return m_params9.MultiSampleType;
    }

    DWORD GetD3D9CreationFlags() const {
      return m_creationFlags9;
    }

    void SetD3D9Device(Com<d3d9::IDirect3DDevice9>&& device9) {
      m_device9 = device9;
    }

    d3d9::IDirect3DDevice9* GetD3D9Device() const {
      return m_device9.ptr();
    }

    void SetD3D7Device(D3D7Device* device7) {
      m_device7 = device7;
    }

    D3D7Device* GetD3D7Device() const {
      return m_device7;
    }

    void SetD3D6Device(D3D6Device* device6) {
      m_device6 = device6;
    }

    D3D6Device* GetD3D6Device() const {
      return m_device6;
    }

    void SetD3D5Device(D3D5Device* device5) {
      m_device5 = device5;
    }

    D3D5Device* GetD3D5Device() const {
      return m_device5;
    }

    void SetD3D3Device(D3D3Device* device3) {
      m_device3 = device3;
    }

    D3D3Device* GetD3D3Device() const {
      return m_device3;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

  private:

    bool                        m_inScene             = false;

    DDrawCommonInterface*       m_commonIntf          = nullptr;

    GUID                        m_deviceGUID;
    uint32_t                    m_totalMemory         = 0;
    D3DMATERIALHANDLE           m_materialHandle      = 0;
    D3DTEXTUREHANDLE            m_textureHandle       = 0;

    // D3DRENDERSTATE_COLORKEYENABLE
    DWORD                       m_colorKeyEnable      = FALSE;
    // D3DRENDERSTATE_COLORKEYBLENDENABLE
    DWORD                       m_colorKeyBlendEnable = FALSE;
    // D3DRENDERSTATE_ANTIALIAS
    DWORD                       m_antialias           = D3DANTIALIAS_NONE;
    // D3DRENDERSTATE_LINEPATTERN
    D3DLINEPATTERN              m_linePattern         = { };
    // D3DRENDERSTATE_TEXTUREMAPBLEND
    DWORD                       m_textureMapBlend     = D3DTBLEND_MODULATE;

    d3d9::D3DPRESENT_PARAMETERS m_params9;
    DWORD                       m_creationFlags9      = 0;

    Com<d3d9::IDirect3DDevice9> m_device9;

    D3D7Device*                 m_device7             = nullptr;
    D3D6Device*                 m_device6             = nullptr;
    D3D5Device*                 m_device5             = nullptr;
    D3D3Device*                 m_device3             = nullptr;

    // Track the origin device, as in the device
    // that gets created through a CreateDevice call
    IUnknown*                   m_origin              = nullptr;

  };

}