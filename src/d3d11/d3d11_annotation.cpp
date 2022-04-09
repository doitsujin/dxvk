#include "d3d11_annotation.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../util/util_misc.h"

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

  D3D11UserDefinedAnnotation::D3D11UserDefinedAnnotation(D3D11DeviceContext* ctx)
  : m_container(ctx),
    m_eventDepth(0) {
    if (m_container->IsAnnotationEnabled())
      RegisterUserDefinedAnnotation<true>(this);
  }

  D3D11UserDefinedAnnotation::D3D11UserDefinedAnnotation(const D3D11UserDefinedAnnotation&)
  {
    if (m_container->IsAnnotationEnabled())
      RegisterUserDefinedAnnotation<true>(this);
  }

  D3D11UserDefinedAnnotation::~D3D11UserDefinedAnnotation() {
    if (m_container->IsAnnotationEnabled())
      RegisterUserDefinedAnnotation<false>(this);
  }


  ULONG STDMETHODCALLTYPE D3D11UserDefinedAnnotation::AddRef() {
    return m_container->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE D3D11UserDefinedAnnotation::Release() {
    return m_container->Release();
  }

  
  HRESULT STDMETHODCALLTYPE D3D11UserDefinedAnnotation::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  

  INT STDMETHODCALLTYPE D3D11UserDefinedAnnotation::BeginEvent(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    if (!m_container->IsAnnotationEnabled())
      return -1;

    D3D10DeviceLock lock = m_container->LockContext();

    m_container->EmitCs([color = Color, labelName = dxvk::str::fromws(Name)](DxvkContext *ctx) {
      VkDebugUtilsLabelEXT label;
      label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      label.pNext = nullptr;
      label.pLabelName = labelName.c_str();
      DecodeD3DCOLOR(color, label.color);

      ctx->beginDebugLabel(&label);
    });

    return m_eventDepth++;
  }


  INT STDMETHODCALLTYPE D3D11UserDefinedAnnotation::EndEvent() {
    if (!m_container->IsAnnotationEnabled())
      return -1;

    D3D10DeviceLock lock = m_container->LockContext();

    m_container->EmitCs([](DxvkContext *ctx) {
      ctx->endDebugLabel();
    });

    return m_eventDepth--;
  }


  void STDMETHODCALLTYPE D3D11UserDefinedAnnotation::SetMarker(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    if (!m_container->IsAnnotationEnabled())
      return;

    D3D10DeviceLock lock = m_container->LockContext();

    m_container->EmitCs([color = Color, labelName = dxvk::str::fromws(Name)](DxvkContext *ctx) {
      VkDebugUtilsLabelEXT label;
      label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      label.pNext = nullptr;
      label.pLabelName = labelName.c_str();
      DecodeD3DCOLOR(color, label.color);

      ctx->insertDebugLabel(&label);
    });
  }


  BOOL STDMETHODCALLTYPE D3D11UserDefinedAnnotation::GetStatus() {
    return m_container->IsAnnotationEnabled();
  }

}
