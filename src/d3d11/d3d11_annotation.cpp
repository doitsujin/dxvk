#include "d3d11_annotation.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"

#include "../util/util_misc.h"
#include "../util/util_win32_compat.h"

namespace dxvk {

  template <bool Register>
  static void RegisterUserDefinedAnnotation(IDXVKUserDefinedAnnotation* annotation) {
    using RegistrationFunctionType = void(__stdcall *)(IDXVKUserDefinedAnnotation*);
    static const int16_t RegisterOrdinal = 28257;
    static const int16_t UnregisterOrdinal = 28258;

    HMODULE d3d9Module = ::LoadLibraryA("d3d9.dll");
    if (!d3d9Module) {
      Logger::info("Unable to find d3d9, some annotations may be missed.");
      return;
    }

    const int16_t ordinal = Register ? RegisterOrdinal : UnregisterOrdinal;
    auto registrationFunction = reinterpret_cast<RegistrationFunctionType>(::GetProcAddress(d3d9Module,
      reinterpret_cast<const char*>(static_cast<uintptr_t>(ordinal))));

    if (!registrationFunction) {
      Logger::info("Unable to find DXVK_RegisterAnnotation, some annotations may be missed.");
      return;
    }

    registrationFunction(annotation);
  }


  template<typename ContextType>
  D3D11UserDefinedAnnotation<ContextType>::D3D11UserDefinedAnnotation(
          ContextType*          container,
    const Rc<DxvkDevice>&       dxvkDevice)
  : m_container(container),
    m_annotationsEnabled(dxvkDevice->debugFlags().test(DxvkDebugFlag::Markers)) {
    if (!IsDeferred && m_annotationsEnabled)
      RegisterUserDefinedAnnotation<true>(this);
  }


  template<typename ContextType>
  D3D11UserDefinedAnnotation<ContextType>::~D3D11UserDefinedAnnotation() {
    if (!IsDeferred && m_annotationsEnabled)
      RegisterUserDefinedAnnotation<false>(this);
  }


  template<typename ContextType>
  ULONG STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::AddRef() {
    return m_container->AddRef();
  }

  
  template<typename ContextType>
  ULONG STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::Release() {
    return m_container->Release();
  }

  
  template<typename ContextType>
  HRESULT STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  

  template<typename ContextType>
  INT STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::BeginEvent(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    if (!m_annotationsEnabled || !Name)
      return -1;

    D3D10DeviceLock lock = m_container->LockContext();

    m_container->EmitCs([
      cColor = Color,
      cLabel = dxvk::str::fromws(Name)
    ] (DxvkContext* ctx) {
      ctx->beginDebugLabel(vk::makeLabel(cColor, cLabel.c_str()));
    });

    return m_eventDepth++;
  }


  template<typename ContextType>
  INT STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::EndEvent() {
    if (!m_annotationsEnabled)
      return -1;

    D3D10DeviceLock lock = m_container->LockContext();

    if (!m_eventDepth)
      return 0;

    m_container->EmitCs([] (DxvkContext* ctx) {
      ctx->endDebugLabel();
    });

    return --m_eventDepth;
  }


  template<typename ContextType>
  void STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::SetMarker(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    if (!m_annotationsEnabled || !Name)
      return;

    D3D10DeviceLock lock = m_container->LockContext();

    m_container->EmitCs([
      cColor = Color,
      cLabel = dxvk::str::fromws(Name)
    ] (DxvkContext* ctx) {
      ctx->insertDebugLabel(vk::makeLabel(cColor, cLabel.c_str()));
    });
  }


  template<typename ContextType>
  BOOL STDMETHODCALLTYPE D3D11UserDefinedAnnotation<ContextType>::GetStatus() {
    return m_annotationsEnabled;
  }


  template class D3D11UserDefinedAnnotation<D3D11DeferredContext>;
  template class D3D11UserDefinedAnnotation<D3D11ImmediateContext>;

}
