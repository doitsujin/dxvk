#include "ddraw_clipper.h"

namespace dxvk {

  DDrawClipper::DDrawClipper(
        DDrawCommonInterface* commonIntf,
        Com<IDirectDrawClipper>&& clipperProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawClipper>(pParent, std::move(clipperProxy))
    , m_commonIntf ( commonIntf ) {
    Logger::debug("DDrawClipper: Created a new clipper");
  }

  DDrawClipper::~DDrawClipper() {
    Logger::debug("DDrawClipper: A clipper bites the dust");
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags) {
    if (unlikely(m_isInitialized))
      return DDERR_ALREADYINITIALIZED;

    m_isInitialized = true;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirectDrawClipper))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawClipper::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::GetClipList(LPRECT lpRect, LPRGNDATA lpClipList, LPDWORD lpdwSize) {
    return m_proxy->GetClipList(lpRect, lpClipList, lpdwSize);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::IsClipListChanged(BOOL *lpbChanged) {
    return m_proxy->IsClipListChanged(lpbChanged);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::SetClipList(LPRGNDATA lpClipList, DWORD dwFlags) {
    return m_proxy->SetClipList(lpClipList, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::SetHWnd(DWORD dwFlags, HWND hWnd) {
    HRESULT hr = m_proxy->SetHWnd(dwFlags, hWnd);
    if (unlikely(FAILED(hr)))
      return hr;

    // The Eschalon: Book I launcher creates a device before attaching the clipper to
    // the primary surface, but right after it sets a HWND on the newly created clipper
    if (unlikely(m_commonIntf != nullptr && m_commonIntf->GetHWND() == nullptr)) {
      // Only set the cached hWnd if it's absent
      m_commonIntf->SetHWND(hWnd);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::GetHWnd(HWND *lphWnd) {
    return m_proxy->GetHWnd(lphWnd);
  }

}

