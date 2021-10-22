#include "d3d11_fence.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11Fence::D3D11Fence(
          D3D11Device*        pDevice,
          UINT64              InitialValue,
          D3D11_FENCE_FLAG    Flags)
  : D3D11DeviceChild<ID3D11Fence>(pDevice) {
    DxvkFenceCreateInfo fenceInfo;
    fenceInfo.initialValue = InitialValue;

    if (Flags)
      Logger::err(str::format("Fence flags 0x", std::hex, Flags, " not supported"));

    m_fence = pDevice->GetDXVKDevice()->createFence(fenceInfo);
  }


  D3D11Fence::~D3D11Fence() {

  }


  HRESULT STDMETHODCALLTYPE D3D11Fence::QueryInterface(
          REFIID              riid,
          void**              ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Fence)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D11Fence: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11Fence::CreateSharedHandle(
    const SECURITY_ATTRIBUTES* pAttributes,
          DWORD               dwAccess,
          LPCWSTR             lpName,
          HANDLE*             pHandle) {
    Logger::err("D3D11Fence::CreateSharedHandle: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11Fence::SetEventOnCompletion(
          UINT64              Value,
          HANDLE              hEvent) {
    m_fence->enqueueWait(Value, [hEvent] {
      SetEvent(hEvent);
    });

    return S_OK;
  }


  UINT64 STDMETHODCALLTYPE D3D11Fence::GetCompletedValue() {
    return m_fence->getValue();
  }
  
}
