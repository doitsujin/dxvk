#pragma once 

/** Implements IDirect3DResource8
* 
* - SetPrivateData, GetPrivateData, FreePrivateData
* - SetPriority, GetPriority
* 
* - Subclasses must provide: PreLoad, GetType
*/

#include "d3d8_device_child.h"

#include "../util/com/com_private_data.h"

namespace dxvk {
  
  template <typename... Type>
  class D3D8Resource : public D3D8DeviceChild<Type...> {

  public:

    D3D8Resource(D3D8DeviceEx* pDevice)
      : D3D8DeviceChild<Type...>(pDevice)
      , m_priority              ( 0 ) { }

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID     refguid,
      const void*       pData,
            DWORD       SizeOfData,
            DWORD       Flags) final {
      HRESULT hr;
      if (Flags & D3DSPD_IUNKNOWN) {
        IUnknown* unknown =
          const_cast<IUnknown*>(
            reinterpret_cast<const IUnknown*>(pData));
        hr = m_privateData.setInterface(
          refguid, unknown);
      }
      else
        hr = m_privateData.setData(
          refguid, SizeOfData, pData);

      if (FAILED(hr))
        return D3DERR_INVALIDCALL;

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID     refguid,
            void*       pData,
            DWORD*      pSizeOfData) final {
      HRESULT hr = m_privateData.getData(
        refguid, reinterpret_cast<UINT*>(pSizeOfData), pData);

      if (FAILED(hr))
        return D3DERR_INVALIDCALL;

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) final {
      HRESULT hr = m_privateData.setData(refguid, 0, nullptr);

      if (FAILED(hr))
        return D3DERR_INVALIDCALL;

      return D3D_OK;
    }

    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) {
      DWORD oldPriority = m_priority;
      m_priority = PriorityNew;
      return oldPriority;
    }

    DWORD STDMETHODCALLTYPE GetPriority() {
      return m_priority;
    }


  protected:

    DWORD m_priority;

  private:

    ComPrivateData m_privateData;

  };


} // namespace dxvk