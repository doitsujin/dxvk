#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class DDrawClassFactory : public ComObjectClamp<IClassFactory> {

  using FunctionType = HRESULT(&)(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);

  public:

    DDrawClassFactory(FunctionType createInstance);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

  private:

    FunctionType m_createInstance;

  };

}