#pragma once

#include <atomic>

#include "com_include.h"

#include "../util_likely.h"
  
namespace dxvk {
  
  template<typename T>
  class NoWrapper : public T {

  public:

    virtual ~NoWrapper() { }

  };
  
  /**
   * \brief Reference-counted COM object
   *
   * This can serve as a templated base class for most
   * COM objects. It implements AddRef and Release from
   * \c IUnknown, and provides methods to increment and
   * decrement private references which are not visible
   * to the application.
   * 
   * Having two reference counters is sadly necessary
   * in order to not break games which steal internal
   * references if the refefence count of an object is
   + greater than they expect. DXVK sometimes requires
   * holding on to objects which the application wants
   * to delete.
   */
  template<typename Base>
  class ComObject : public Base {
    
  public:
    
    virtual ~ComObject() { }
    
    ULONG STDMETHODCALLTYPE AddRef() {
      uint32_t refCount = m_refCount++;
      if (unlikely(!refCount))
        AddRefPrivate();
      return refCount + 1;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      uint32_t refCount = --m_refCount;
      if (unlikely(!refCount))
        ReleasePrivate();
      return refCount;
    }


    void AddRefPrivate() {
      ++m_refPrivate;
    }


    void ReleasePrivate() {
      uint32_t refPrivate = --m_refPrivate;
      if (unlikely(!refPrivate)) {
        m_refPrivate += 0x80000000;
        delete this;
      }
    }

    ULONG GetPrivateRefCount() {
      return m_refPrivate.load();
    }
    
  protected:
    
    std::atomic<uint32_t> m_refCount   = { 0ul };
    std::atomic<uint32_t> m_refPrivate = { 0ul };
    
  };

  /**
   * \brief Clamped, reference-counted COM object
   *
   * This version of ComObject ensures that the reference
   * count does not wrap around if a release happens at zero.
   * eg. [m_refCount = 0]
   *     Release()
   *     [m_refCount = 0]
   * This is a notable quirk of D3D9's COM implementation
   * and is relied upon by some games.
   */
  template<typename Base>
  class ComObjectClamp : public ComObject<Base> {

  public:

    ULONG STDMETHODCALLTYPE Release() {
      ULONG refCount = this->m_refCount;
      if (likely(refCount != 0ul)) {
        this->m_refCount--;
        refCount--;

        if (refCount == 0ul)
          this->ReleasePrivate();
      }

      return refCount;
    }

  };
  
  template<typename T>
  inline void InitReturnPtr(T** ptr) {
    if (ptr != nullptr)
      *ptr = nullptr;
  }
  
}
