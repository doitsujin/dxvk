#include "d3d8_device.h"

#include "d3d8_interface.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

namespace dxvk {

  D3D8DeviceEx::D3D8DeviceEx(
    D3D8InterfaceEx*              pParent,
    Com<d3d9::IDirect3DDevice9>&& pDevice,
    //D3D8Adapter*                  pAdapter,
    D3DDEVTYPE                    DeviceType,
    HWND                          hFocusWindow,
    DWORD                         BehaviorFlags)
    : D3D8DeviceBase(std::move(pDevice))
    , m_parent(pParent)
    , m_deviceType(DeviceType)
    , m_window(hFocusWindow)
    , m_behaviorFlags(BehaviorFlags) {

  }


  D3D8DeviceEx::~D3D8DeviceEx() {
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::QueryInterface(REFIID riid, void** ppvObject) {

    return S_OK;
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
    //  || riid == IID_IDirect3DDevice8) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D8DeviceEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::TestCooperativeLevel() {
    // Equivelant of D3D11/DXGI present tests. We can always present.
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetDirect3D(IDirect3D8** ppD3D8) {
    if (ppD3D8 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D8 = m_parent.ref();
    return D3D_OK;
  }

} // namespace dxvk