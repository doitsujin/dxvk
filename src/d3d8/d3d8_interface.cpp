#include "d3d8_interface.h"

#include "d3d8_device.h"
#include "d3d8_texture.h"

#include <cstring>

namespace dxvk
{
  D3D8InterfaceEx::D3D8InterfaceEx(UINT SDKVersion)
  {
    d3d9::Direct3DCreate9Ex(D3D_SDK_VERSION, &m_d3d9ex);

    m_adapterCount = m_d3d9ex->GetAdapterCount();

    m_adapterModeCounts.resize(m_adapterCount);
    m_adapterModes.reserve(m_adapterCount);

    for (UINT adapter = 0; adapter < m_adapterCount; adapter++) {

      m_adapterModes.emplace_back();

      // cache adapter modes and mode counts for each d3d9 format
      for (d3d9::D3DFORMAT fmt : ADAPTER_FORMATS) {

        const int modeCount = m_d3d9ex->GetAdapterModeCount(adapter, fmt);

        for (UINT mode = 0; mode < modeCount; mode++) {

          m_adapterModes[adapter].emplace_back();
          
          m_d3d9ex->EnumAdapterModes(adapter, fmt, mode, &(m_adapterModes[adapter].back()));

          // can't use modeCount as it's only for one fmt
          m_adapterModeCounts[adapter]++;
        }
      }
    }
  }

  HRESULT STDMETHODCALLTYPE D3D8InterfaceEx::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
      //|| riid == __uuidof(IDirect3D8)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D8InterfaceEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D8InterfaceEx::GetAdapterIdentifier(
          UINT Adapter,
          DWORD Flags,
          D3DADAPTER_IDENTIFIER8* pIdentifier) {

    // This flag now has the opposite effect.
    // Either way, WHQLevel will be 1 with Direct3D9Ex
    if (Flags & D3DENUM_NO_WHQL_LEVEL)
      Flags &= ~D3DENUM_WHQL_LEVEL;
    else
      Flags |= D3DENUM_WHQL_LEVEL;

    d3d9::D3DADAPTER_IDENTIFIER9 identifier9;
    HRESULT res = m_d3d9ex->GetAdapterIdentifier(Adapter, Flags, &identifier9);

    strncpy(pIdentifier->Driver, identifier9.Driver, MAX_DEVICE_IDENTIFIER_STRING);
    strncpy(pIdentifier->Description, identifier9.Description, MAX_DEVICE_IDENTIFIER_STRING);

    pIdentifier->DriverVersion = identifier9.DriverVersion;
    pIdentifier->VendorId = identifier9.VendorId;
    pIdentifier->DeviceId = identifier9.DeviceId;
    pIdentifier->SubSysId = identifier9.SubSysId;
    pIdentifier->Revision = identifier9.Revision;

    // copy
    pIdentifier->DeviceIdentifier = identifier9.DeviceIdentifier;

    pIdentifier->WHQLLevel = identifier9.WHQLLevel;
    
    return res;
  }

  HRESULT __stdcall D3D8InterfaceEx::EnumAdapterModes(
          UINT Adapter,
          UINT Mode,
          D3DDISPLAYMODE* pMode) {

    if (Adapter >= m_adapterCount || Mode >= m_adapterModeCounts[Adapter] || pMode == nullptr) {
      return D3DERR_INVALIDCALL;
    }

    pMode->Width = m_adapterModes[Adapter][Mode].Width;
    pMode->Height = m_adapterModes[Adapter][Mode].Height;
    pMode->RefreshRate = m_adapterModes[Adapter][Mode].RefreshRate;
    pMode->Format = (D3DFORMAT)m_adapterModes[Adapter][Mode].Format;
    return D3D_OK;
  }

  HRESULT __stdcall D3D8InterfaceEx::CreateDevice(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice8** ppReturnedDeviceInterface) {

    Com<d3d9::IDirect3DDevice9> pDevice9 = nullptr;
    d3d9::D3DPRESENT_PARAMETERS params = ConvertPresentParameters9(pPresentationParameters);
    HRESULT res = m_d3d9ex->CreateDevice(
      Adapter,
      (d3d9::D3DDEVTYPE)DeviceType,
      hFocusWindow,
      BehaviorFlags,
      &params,
      &pDevice9
    );

    *ppReturnedDeviceInterface = ref(new D3D8DeviceEx(this, std::move(pDevice9), DeviceType, hFocusWindow, BehaviorFlags));

    return res;
  }



} // namespace dxvk