#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class D3D9DeviceEx;

  class D3D9On12 final : public IDirect3DDevice9On12 {

  public:

    D3D9On12(D3D9DeviceEx* device);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE GetD3D12Device(REFIID riid, void** object);
    HRESULT STDMETHODCALLTYPE UnwrapUnderlyingResource(IDirect3DResource9* resource, ID3D12CommandQueue* command_queue, REFIID riid, void** object);
    HRESULT STDMETHODCALLTYPE ReturnUnderlyingResource(IDirect3DResource9* resource, UINT num_sync, UINT64* signal_values, ID3D12Fence** fences);

  private:
    
    D3D9DeviceEx* m_device;

  };

}
