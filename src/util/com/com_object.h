#pragma once

#include <atomic>

#include "com_include.h"
  
namespace dxvk {
  
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
  template<typename... Base>
  class ComObject : public Base... {
    
  public:
    
    virtual ~ComObject() { }
    
    ULONG STDMETHODCALLTYPE AddRef() {
      ULONG refCount = m_refCount++;
      if (refCount == 0ul)
        AddRefPrivate();
      return refCount;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      ULONG refCount = --m_refCount;
      if (refCount == 0ul)
        ReleasePrivate();
      return refCount;
    }


    void AddRefPrivate() {
      ++m_refPrivate;
    }


    void ReleasePrivate() {
      if (--m_refPrivate == 0ul) {
        m_refPrivate += 0x80000000;
        delete this;
      }
    }
    
  private:
    
    std::atomic<ULONG> m_refCount   = { 0ul };
    std::atomic<ULONG> m_refPrivate = { 0ul };
    
  };
  
  template<typename T>
  inline void InitReturnPtr(T** ptr) {
    if (ptr != nullptr)
      *ptr = nullptr;
  }
  
}
