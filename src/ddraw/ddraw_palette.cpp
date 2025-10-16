#include "ddraw_palette.h"

#include "ddraw_common_surface.h"

namespace dxvk {

  std::atomic<uint32_t> DDrawPalette::s_paletteCount = 0;

  DDrawPalette::DDrawPalette(
        Com<IDirectDrawPalette>&& paletteProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawPalette>(pParent, std::move(paletteProxy)) {
    if (m_parent != nullptr)
      m_parent->AddRef();

    m_paletteCount = ++s_paletteCount;

    Logger::debug(str::format("DDrawPalette: Created a new palette nr. [[1-", m_paletteCount, "]]"));
  }

  DDrawPalette::~DDrawPalette() {
    if (m_commonSurf != nullptr)
      m_commonSurf->SetPalette(nullptr);

    if (m_parent != nullptr)
      m_parent->Release();

    Logger::debug(str::format("DDrawPalette: Palette nr. [[1-", m_paletteCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirectDrawPalette))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawPalette::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "Because the DirectDrawPalette object is initialized when
  // it is created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDrawPalette::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::GetCaps(LPDWORD lpdwCaps) {
    return m_proxy->GetCaps(lpdwCaps);
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) {
    return m_proxy->GetEntries(dwFlags, dwBase, dwNumEntries, lpEntries);
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) {
    return m_proxy->SetEntries(dwFlags, dwStartingEntry, dwCount, lpEntries);
  }

}

