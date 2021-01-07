#include "d3d11_state_object.h"

namespace dxvk {

  D3D11DeviceContextState::D3D11DeviceContextState(
          D3D11Device*         pDevice)
  : D3D11DeviceChild<ID3DDeviceContextState>(pDevice) {

  }

  
  D3D11DeviceContextState::~D3D11DeviceContextState() {

  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContextState::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;
    
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3DDeviceContextState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D11DeviceContextState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

}
