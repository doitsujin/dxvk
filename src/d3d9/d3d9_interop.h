#pragma once

#include "d3d9_interfaces.h"

namespace dxvk {

  class D3D9InterfaceEx;

  class D3D9VkInteropInterface final : public ID3D9VkInteropInterface {

  public:

    D3D9VkInteropInterface(
            D3D9InterfaceEx*      pInterface);

    ~D3D9VkInteropInterface();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);

    void STDMETHODCALLTYPE GetInstanceHandle(
            VkInstance*           pInstance);

    void STDMETHODCALLTYPE GetPhysicalDeviceHandle(
            UINT                  Adapter,
            VkPhysicalDevice*     pPhysicalDevice);

  private:

    D3D9InterfaceEx* m_interface;

  };

}
