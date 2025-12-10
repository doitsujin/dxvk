#pragma once

#include "d3d11_include.h"
#include "d3d11_state.h"

#include "../util/com/com_private_data.h"

namespace dxvk {

  class D3D11Device;

  template<typename Base>
  class D3D11DeviceObject : public Base {
    
  public:

    D3D11DeviceObject(D3D11Device* pDevice)
    : m_parent(pDevice) {

    }
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID               guid,
            UINT*                 pDataSize,
            void*                 pData) final {
      return m_privateData.getData(
        guid, pDataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID               guid,
            UINT                  DataSize,
      const void*                 pData) final {
      // WKPDID_D3DDebugObjectName, can't use directly due to MSVC link errors
      if (guid == GUID{0x429b8c22,0x9188,0x4b0c,0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00})
        SetDebugName(static_cast<const char*>(pData));

      return m_privateData.setData(
        guid, DataSize, pData);
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID               guid,
      const IUnknown*             pUnknown) final {
      return m_privateData.setInterface(
        guid, pUnknown);
    }

    void STDMETHODCALLTYPE GetDevice(
            ID3D11Device**        ppDevice) final {
      *ppDevice = ref(GetParentInterface());
    }

    virtual void STDMETHODCALLTYPE SetDebugName(const char* pName) {
      // No-op by default
    }

  protected:

    ID3D11Device* GetParentInterface() const {
      // We don't know the definition of ID3D11Device
      // here, because D3D11Device includes this file.
      return reinterpret_cast<ID3D11Device*>(m_parent);
    }

    D3D11Device* const m_parent;
    
  private:
    
    ComPrivateData m_privateData;
    
  };

  
  template<typename Base>
  class D3D11DeviceChild : public D3D11DeviceObject<ComObject<Base>> {
    
  public:

    D3D11DeviceChild(D3D11Device* pDevice)
    : D3D11DeviceObject<ComObject<Base>>(pDevice) {

    }

    ULONG STDMETHODCALLTYPE AddRef() {
      uint32_t refCount = this->m_refCount++;
      if (unlikely(!refCount)) {
        this->AddRefPrivate();
        this->GetParentInterface()->AddRef();
      }

      return refCount + 1;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      uint32_t refCount = --this->m_refCount;
      if (unlikely(!refCount)) {
        auto* parent = this->GetParentInterface();
        this->ReleasePrivate();
        parent->Release();
      }
      return refCount;
    }
    
  };

  template<typename Base, typename Self>
  class D3D11StateObject : public D3D11DeviceObject<Base> {
    using Container = D3D11StateObjectSet<Self>;

    constexpr static uint32_t AddRefValue = 1u;
    constexpr static uint32_t ReleaseShift = 16u;
    constexpr static uint32_t ReleaseValue = 1u << ReleaseShift;
    constexpr static uint32_t RefMask = ReleaseValue - 1u;
  public:

    D3D11StateObject(D3D11Device* pDevice, Container* pContainer)
    : D3D11DeviceObject<Base>(pDevice), m_container(pContainer) {

    }

    ULONG STDMETHODCALLTYPE AddRef() {
      uint32_t refCount = m_refCount.fetch_add(1u, std::memory_order_acquire);

      if (unlikely(!refCount)) {
        AddRefPrivate();
        this->GetParentInterface()->AddRef();
      }

      return refCount + 1;
    }

    ULONG STDMETHODCALLTYPE Release() {
      uint32_t refCount = m_refCount.fetch_sub(1u, std::memory_order_release) - 1u;

      if (unlikely(!refCount)) {
        ID3D11Device* device = this->GetParentInterface();
        ReleasePrivate();
        device->Release();
      }

      return refCount;
    }

    void AddRefPrivate() {
      // Since state objects manage themselves inside a look-up table, we need to
      // atomically count both release and addrefs to support the following sequence
      // of events:
      // - Thread 0: Calls StateObjectSet::Create and takes lock
      // - Thread 1: Calls StateObjectSet::Destroy, is now blocked
      // - Thread 0: StateObjectSet::Create returns
      // - Thread 0: Calls StateObjectSet::Destroy immmediately and takes lock
      // - Thread 0: StateObjectSet::Destroy returns
      // - Thread 1: Gets unblocked
      // - Thread 1: StateObjectSet::Destroy returns
      // In this scenario, only one thread can safely destroy the object.
      uint32_t expected = m_refPrivate.load(std::memory_order_relaxed);
      uint32_t desired;

      do {
        desired = ((expected + 1u) & RefMask) | (expected & ~RefMask);
      } while (!m_refPrivate.compare_exchange_strong(expected, desired, std::memory_order_acquire));
    }

    void ReleasePrivate() {
      uint32_t refCount = m_refPrivate.fetch_add(ReleaseValue, std::memory_order_release) + ReleaseValue;

      uint32_t addRefCount = (refCount & RefMask) / AddRefValue;
      uint32_t releaseCount = (refCount & ~RefMask) / ReleaseValue;

      if (unlikely(addRefCount == releaseCount))
        m_container->Destroy(static_cast<Self*>(this), refCount);
    }

    BOOL IsCurrent(uint32_t version) {
      return m_refPrivate.load(std::memory_order_relaxed) == version;
    }

  private:

    std::atomic<uint32_t> m_refCount = { 0u };
    std::atomic<uint32_t> m_refPrivate = { 0u };
    Container*            m_container = nullptr;
    
  };
  
}
