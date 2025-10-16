#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"

namespace dxvk {

  class DDrawCommonSurface;

  class DDrawPalette final : public DDrawWrappedObject<IUnknown, IDirectDrawPalette> {

  public:

    DDrawPalette(
          Com<IDirectDrawPalette>&& paletteProxy,
          IUnknown* pParent);

    ~DDrawPalette();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDWORD lpdwCaps);

    HRESULT STDMETHODCALLTYPE GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries);

    HRESULT STDMETHODCALLTYPE SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries);

    void SetCommonSurface(DDrawCommonSurface* commonSurf) {
      m_commonSurf = commonSurf;
    }

  private:

    DDrawCommonSurface* m_commonSurf   = nullptr;

    uint32_t            m_paletteCount = 0;
    static std::atomic<uint32_t> s_paletteCount;

  };

}