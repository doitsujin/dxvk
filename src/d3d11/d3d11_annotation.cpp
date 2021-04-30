#include "d3d11_annotation.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../util/util_misc.h"

namespace dxvk {

  D3D11UserDefinedAnnotation::D3D11UserDefinedAnnotation(D3D11DeviceContext* ctx)
  : m_container(ctx),
    m_eventDepth(0) { }


  D3D11UserDefinedAnnotation::~D3D11UserDefinedAnnotation() {

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
