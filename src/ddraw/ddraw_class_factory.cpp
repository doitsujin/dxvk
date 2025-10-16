#include "ddraw_class_factory.h"

namespace dxvk {

  DDrawClassFactory::DDrawClassFactory(FunctionType createInstance)
    : m_createInstance ( createInstance ) {
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    return m_createInstance(pUnkOuter, riid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::LockServer(BOOL fLock) {
    Logger::warn("!!! DDrawClassFactory::LockServer: Stub");
    return S_OK;
  }

}