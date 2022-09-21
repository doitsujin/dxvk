#include "d3d9_interop.h"
#include "d3d9_interface.h"

namespace dxvk {

  ////////////////////////////////
  // Interface Interop
  ///////////////////////////////

  D3D9VkInteropInterface::D3D9VkInteropInterface(
          D3D9InterfaceEx*      pInterface)
    : m_interface(pInterface) {

  }

  D3D9VkInteropInterface::~D3D9VkInteropInterface() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetInstanceHandle(
          VkInstance*           pInstance) {
    if (pInstance != nullptr)
      *pInstance = m_interface->GetInstance()->handle();
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetPhysicalDeviceHandle(
          UINT                  Adapter,
          VkPhysicalDevice*     pPhysicalDevice) {
    if (pPhysicalDevice != nullptr) {
      D3D9Adapter* adapter = m_interface->GetAdapter(Adapter);
      *pPhysicalDevice = adapter ? adapter->GetDXVKAdapter()->handle() : nullptr;
    }
  }

}
