#include "d3d11_fence.h"
#include "d3d11_device.h"
#include "../util/util_win32_compat.h"

namespace dxvk {
  
  D3D11Fence::D3D11Fence(
          D3D11Device*        pDevice,
          UINT64              InitialValue,
          D3D11_FENCE_FLAG    Flags,
          HANDLE              hFence)
  : D3D11DeviceChild<ID3D11Fence>(pDevice) {
    DxvkFenceCreateInfo fenceInfo;
    fenceInfo.initialValue = InitialValue;
    m_flags = Flags;

    if (Flags & D3D11_FENCE_FLAG_SHARED) {
      fenceInfo.sharedType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT;
      if (hFence == nullptr)
        hFence = INVALID_HANDLE_VALUE;
      fenceInfo.sharedHandle = hFence;
    }

    if (Flags & ~D3D11_FENCE_FLAG_SHARED)
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

    if (logQueryInterfaceError(__uuidof(ID3D11Fence), riid)) {
      Logger::warn("D3D11Fence: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11Fence::CreateSharedHandle(
    const SECURITY_ATTRIBUTES* pAttributes,
          DWORD               dwAccess,
          LPCWSTR             lpName,
          HANDLE*             pHandle) {
    if (!(m_flags & D3D11_FENCE_FLAG_SHARED))
      return E_INVALIDARG;

    if (pAttributes)
      Logger::warn(str::format("CreateSharedHandle: attributes ", pAttributes, " not handled"));
    if (dwAccess)
      Logger::warn(str::format("CreateSharedHandle: access ", dwAccess, " not handled"));
    if (lpName)
      Logger::warn(str::format("CreateSharedHandle: name ", dxvk::str::fromws(lpName), " not handled"));

    HANDLE sharedHandle = m_fence->sharedHandle();
    if (sharedHandle == INVALID_HANDLE_VALUE)
      return E_INVALIDARG;

    *pHandle = sharedHandle;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11Fence::SetEventOnCompletion(
          UINT64              Value,
          HANDLE              hEvent) {
    if (hEvent) {
      m_fence->enqueueWait(Value, [hEvent] {
        SetEvent(hEvent);
      });
    } else {
      m_fence->wait(Value);
    }
    return S_OK;
  }


  UINT64 STDMETHODCALLTYPE D3D11Fence::GetCompletedValue() {
    // TODO in the case of rewinds, the stored value may be higher.
    // For shared fences, calling vkGetSemaphoreCounterValue here could alleviate the issue.

    return m_fence->getValue();
  }
  
}
