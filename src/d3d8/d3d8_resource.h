#pragma once 

/** Implements IDirect3DResource8
* 
* - SetPrivateData, GetPrivateData, FreePrivateData
* - SetPriority, GetPriority
* 
* - Subclasses provide: PreLoad, GetType
*/

#include "d3d8_device_child.h"
#include "../util/com/com_private_data.h"

namespace dxvk {
  
  template <typename D3D9, typename D3D8>
  class D3D8Resource : public D3D8DeviceChild<D3D9, D3D8> {

  public:

    D3D8Resource(D3D8Device* pDevice, Com<D3D9>&& Object)
      : D3D8DeviceChild<D3D9, D3D8>(pDevice, std::move(Object))
      , m_priority                 ( 0 ) { }

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID     refguid,
      const void*       pData,
            DWORD       SizeOfData,
            DWORD       Flags) final {
      HRESULT hr;
      if (Flags & D3DSPD_IUNKNOWN) {
        if(unlikely(SizeOfData != sizeof(IUnknown*)))
          return D3DERR_INVALIDCALL;
        IUnknown* unknown =
          const_cast<IUnknown*>(
            reinterpret_cast<const IUnknown*>(pData));
        hr = m_privateData.setInterface(
          refguid, unknown);
      }
      else
        hr = m_privateData.setData(
          refguid, SizeOfData, pData);

      if (unlikely(FAILED(hr)))
        return D3DERR_INVALIDCALL;

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID     refguid,
            void*       pData,
            DWORD*      pSizeOfData) final {
      if (unlikely(pData == nullptr && pSizeOfData == nullptr))
        return D3DERR_NOTFOUND;

      HRESULT hr = m_privateData.getData(
        refguid, reinterpret_cast<UINT*>(pSizeOfData), pData);

      if (unlikely(FAILED(hr))) {
        if(hr == DXGI_ERROR_MORE_DATA)
          return D3DERR_MOREDATA;
        else if (hr == DXGI_ERROR_NOT_FOUND)
          return D3DERR_NOTFOUND;
        else
          return D3DERR_INVALIDCALL;
      }

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) final {
      HRESULT hr = m_privateData.setData(refguid, 0, nullptr);

      if (unlikely(FAILED(hr)))
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

    virtual IUnknown* GetInterface(REFIID riid) override try {
      return D3D8DeviceChild<D3D9, D3D8>::GetInterface(riid);
    } catch (HRESULT err) {
      if (riid == __uuidof(IDirect3DResource8))
        return this;
      
      throw err;
    }

  protected:

    DWORD m_priority;

  private:

    ComPrivateData m_privateData;

  };


}