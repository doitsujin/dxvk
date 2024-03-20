#include "d3d9_on_12.h"

#include "d3d9_device.h"

namespace dxvk {

  D3D9On12::D3D9On12(D3D9DeviceEx* device)
    : m_device(device) {

  }

  HRESULT STDMETHODCALLTYPE D3D9On12::QueryInterface(REFIID riid, void** object) {
    return m_device->QueryInterface(riid, object);
  }
  ULONG STDMETHODCALLTYPE D3D9On12::AddRef() {
    return m_device->AddRef();
  }
  ULONG STDMETHODCALLTYPE D3D9On12::Release() {
    return m_device->Release();
  }

  HRESULT STDMETHODCALLTYPE D3D9On12::GetD3D12Device(REFIID riid, void** object) {
    InitReturnPtr(object);

    Logger::err("D3D9On12::GetD3D12Device: Stub");
    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE D3D9On12::UnwrapUnderlyingResource(IDirect3DResource9* resource, ID3D12CommandQueue* command_queue, REFIID riid, void** object) {
    Logger::err("D3D9On12::GetD3D12Device: UnwrapUnderlyingResource: Stub");
    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE D3D9On12::ReturnUnderlyingResource(IDirect3DResource9* resource, UINT num_sync, UINT64* signal_values, ID3D12Fence** fences) {
    if (num_sync)
      Logger::err("D3D9On12::GetD3D12Device: ReturnUnderlyingResource: Stub");

    m_device->FlushAndSync9On12();
    return S_OK;
  }

}
