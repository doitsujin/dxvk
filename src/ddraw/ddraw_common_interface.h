#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"

#include <unordered_set>
#include <unordered_map>

namespace dxvk {

  class D3DCommonDevice;
  class D3DCommonTexture;

  class DDrawCommonSurface;

  class D3D3Interface;

  class DDraw7Interface;
  class DDraw4Interface;
  class DDraw2Interface;
  class DDrawInterface;

  class DDraw4Surface;
  class DDrawSurface;

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface(const D3DOptions& options);

    ~DDrawCommonInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    D3D3Interface* GetOrCreateD3D3Interface();

    static bool IsWrappedSurface(IDirectDrawSurface* surface);

    static void AddWrappedSurface(IDirectDrawSurface* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface* surface);

    static bool IsWrappedSurface(IDirectDrawSurface2* surface);

    static void AddWrappedSurface(IDirectDrawSurface2* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface2* surface);

    static bool IsWrappedSurface(IDirectDrawSurface3* surface);

    static void AddWrappedSurface(IDirectDrawSurface3* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface3* surface);

    static bool IsWrappedSurface(IDirectDrawSurface4* surface);

    static void AddWrappedSurface(IDirectDrawSurface4* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface4* surface);

    static bool IsWrappedSurface(IDirectDrawSurface7* surface);

    static void AddWrappedSurface(IDirectDrawSurface7* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface7* surface);

    static DDraw4Surface* GetSurface4FromTextureHandle(D3DTEXTUREHANDLE handle);

    static DDrawSurface* GetSurfaceFromTextureHandle(D3DTEXTUREHANDLE handle);

    static D3DTEXTUREHANDLE GetNextTextureHandle() {
      return ++s_textureHandle;
    }

    static void EmplaceTexture(D3DCommonTexture* commonTex, D3DTEXTUREHANDLE handle) {
      s_textures.emplace(std::piecewise_construct,
                         std::forward_as_tuple(handle),
                         std::forward_as_tuple(commonTex));
    }

    static void ReleaseTextureHandle(D3DTEXTUREHANDLE handle) {
      auto textureIter = s_textures.find(handle);

      if (likely(textureIter != s_textures.end()))
        s_textures.erase(textureIter);
    }

    void MarkAsInitialized() {
      m_isInitialized = true;
    }

    bool IsInitialized() const {
      return m_isInitialized;
    }

    void SetWaitForVBlank(bool waitForVBlank) {
      m_waitForVBlank = waitForVBlank;
    }

    bool GetWaitForVBlank() const {
      return m_waitForVBlank;
    }

    void SetCommonD3DDevice(D3DCommonDevice* commonD3DDevice) {
      m_commonD3DDevice = commonD3DDevice;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    void SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
      m_hWnd = hWnd;
      m_cooperativeLevel = dwFlags;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    bool IsCooperativeLevelSet() const {
      return (m_cooperativeLevel & DDSCL_NORMAL) ||
             (m_cooperativeLevel & DDSCL_EXCLUSIVE);
    }

    // Used to hook the hWnd during SetClipper calls,
    // and use that on device creation if no other
    // hWnd is specified through SetCooperativeLevel
    void SetHWND(HWND hWnd) {
      if (unlikely(m_hWnd == nullptr && hWnd != nullptr))
        m_hWnd = hWnd;
    }

    HWND GetHWND() const {
      return m_hWnd;
    }

    void SetPrimarySurface(DDrawCommonSurface* ps) {
      m_ps = ps;
    }

    DDrawCommonSurface* GetPrimarySurface() {
      return m_ps;
    }

    DDrawModeSize* GetModeSize() {
      return &m_modeSize;
    }

    void SetAdapterIdentifier(const d3d9::D3DADAPTER_IDENTIFIER9& adapterIdentifier9) {
      m_adapterIdentifier9 = adapterIdentifier9;
    }

    const d3d9::D3DADAPTER_IDENTIFIER9* GetAdapterIdentifier() const {
      return &m_adapterIdentifier9;
    }

    const D3DOptions* GetOptions() const {
      return &m_d3dOptions;
    }

    void SetD3D3Interface(D3D3Interface* d3d3Intf) {
      m_d3d3Intf = d3d3Intf;
    }

    D3D3Interface* GetD3D3Interface() const {
      return m_d3d3Intf;
    }

    void SetDD7Interface(DDraw7Interface* intf7) {
      m_intf7 = intf7;
    }

    DDraw7Interface* GetDD7Interface() const {
      return m_intf7;
    }

    void SetDD4Interface(DDraw4Interface* intf4) {
      m_intf4 = intf4;
    }

    DDraw4Interface* GetDD4Interface() const {
      return m_intf4;
    }

    void SetDD2Interface(DDraw2Interface* intf2) {
      m_intf2 = intf2;
    }

    DDraw2Interface* GetDD2Interface() const {
      return m_intf2;
    }

    void SetDDInterface(DDrawInterface* intf) {
      m_intf = intf;
    }

    DDrawInterface* GetDDInterface() const {
      return m_intf;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

  private:

    bool                              m_isInitialized      = false;
    bool                              m_waitForVBlank      = true;

    D3DCommonDevice*                  m_commonD3DDevice    = nullptr;

    HWND                              m_hWnd               = nullptr;
    DWORD                             m_cooperativeLevel   = 0;

    DDrawCommonSurface*               m_ps                 = nullptr;
    DDrawModeSize                     m_modeSize           = { };

    d3d9::D3DADAPTER_IDENTIFIER9      m_adapterIdentifier9 = { };

    D3DOptions                        m_d3dOptions;

    D3D3Interface*                    m_d3d3Intf           = nullptr;

    DDraw7Interface*                  m_intf7              = nullptr;
    DDraw4Interface*                  m_intf4              = nullptr;
    DDraw2Interface*                  m_intf2              = nullptr;
    DDrawInterface*                   m_intf               = nullptr;

    // Track the origin surface, as in the DDraw surface
    // that gets created through a DirectDrawCreate(Ex) call
    IUnknown*                         m_origin             = nullptr;

    // Tests have indicated that once created, texture handles are shared across
    // all devices and DDraw interfaces, regardless of their relation
    static std::atomic<D3DTEXTUREHANDLE> s_textureHandle;
    static inline std::unordered_map<D3DTEXTUREHANDLE, D3DCommonTexture*> s_textures;

    // Keep wrapped surface tracking shared between all DDraw interfaces as tests
    // as well as some games (e.g. GTA 2) depend on wrapped surface lookups across
    // unrelated DDraw interface objects... yes, really...
    static inline std::unordered_set<IDirectDrawSurface7*> s_surfaces7;
    static inline std::unordered_set<IDirectDrawSurface4*> s_surfaces4;
    static inline std::unordered_set<IDirectDrawSurface3*> s_surfaces3;
    static inline std::unordered_set<IDirectDrawSurface2*> s_surfaces2;
    static inline std::unordered_set<IDirectDrawSurface*>  s_surfaces;

  };

}