#include "d3d9_annotation.h"

namespace dxvk {

  ////////////////////////////
  // D3D9GlobalAnnotationList
  ////////////////////////////

  D3D9GlobalAnnotationList::D3D9GlobalAnnotationList()
    : m_shouldAnnotate(false)
    , m_eventDepth(0)
  {}


  D3D9GlobalAnnotationList::~D3D9GlobalAnnotationList()
  {}


  void D3D9GlobalAnnotationList::RegisterAnnotator(IDXVKUserDefinedAnnotation* annotation) {
    auto lock = std::unique_lock(m_mutex);
    m_shouldAnnotate = true;
    m_annotations.push_back(annotation);
  }


  void D3D9GlobalAnnotationList::UnregisterAnnotator(IDXVKUserDefinedAnnotation* annotation) {
    auto lock = std::unique_lock(m_mutex);
    auto iter = std::find(m_annotations.begin(), m_annotations.end(), annotation);
    if (iter != m_annotations.end())
        m_annotations.erase(iter);
  }


  INT D3D9GlobalAnnotationList::BeginEvent(D3DCOLOR color, LPCWSTR name) {
    if (!m_shouldAnnotate)
      return 0;

    auto lock = std::unique_lock(m_mutex);
    for (auto* annotation : m_annotations)
      annotation->BeginEvent(color, name);

    return m_eventDepth++;
  }


  INT D3D9GlobalAnnotationList::EndEvent() {
    if (!m_shouldAnnotate)
      return 0;

    auto lock = std::unique_lock(m_mutex);
    for (auto* annotation : m_annotations)
      annotation->EndEvent();

    return m_eventDepth--;
  }


  void D3D9GlobalAnnotationList::SetMarker(D3DCOLOR color, LPCWSTR name) {
    if (!m_shouldAnnotate)
      return;

    auto lock = std::unique_lock(m_mutex);
    for (auto* annotation : m_annotations)
      annotation->SetMarker(color, name);
  }


  void D3D9GlobalAnnotationList::SetRegion(D3DCOLOR color, LPCWSTR name) {
    // This, by the documentation, does nothing.
  }


  BOOL D3D9GlobalAnnotationList::QueryRepeatFrame() const {
    // This, by the documentation, does nothing.
    // It's meant to return TRUE if the profiler/debugger
    // wants a frame to be repeated, but we never need that.
    return FALSE;
  }


  void D3D9GlobalAnnotationList::SetOptions(DWORD options) {
    // This is used to say that the app should
    // not be debugged/profiled.
  }


  DWORD D3D9GlobalAnnotationList::GetStatus() const {
    // This returns whether the app is being
    // profiled / debugged.
    // Some apps may rely on this to emit
    // debug markers.
    return m_shouldAnnotate ? 1 : 0;
  }

  ////////////////////////////
  // D3D9UserDefinedAnnotation
  ////////////////////////////

  D3D9UserDefinedAnnotation::D3D9UserDefinedAnnotation(D3D9DeviceEx* device)
    : m_container(device) {
    D3D9GlobalAnnotationList::Instance().RegisterAnnotator(this);
  }


  D3D9UserDefinedAnnotation::~D3D9UserDefinedAnnotation() {
    D3D9GlobalAnnotationList::Instance().UnregisterAnnotator(this);
  }


  ULONG STDMETHODCALLTYPE D3D9UserDefinedAnnotation::AddRef() {
    return m_container->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D9UserDefinedAnnotation::Release() {
    return m_container->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D9UserDefinedAnnotation::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }


  INT STDMETHODCALLTYPE D3D9UserDefinedAnnotation::BeginEvent(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    m_container->EmitCs([color = Color, labelName = dxvk::str::fromws(Name)](DxvkContext *ctx) {
      VkDebugUtilsLabelEXT label;
      label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      label.pNext = nullptr;
      label.pLabelName = labelName.c_str();
      DecodeD3DCOLOR(color, label.color);

      ctx->beginDebugLabel(&label);
    });

    // Handled by the global list.
    return 0;
  }


  INT STDMETHODCALLTYPE D3D9UserDefinedAnnotation::EndEvent() {
    m_container->EmitCs([](DxvkContext *ctx) {
      ctx->endDebugLabel();
    });

    // Handled by the global list.
    return 0;
  }


  void STDMETHODCALLTYPE D3D9UserDefinedAnnotation::SetMarker(
          D3DCOLOR                Color,
          LPCWSTR                 Name) {
    m_container->EmitCs([color = Color, labelName = dxvk::str::fromws(Name)](DxvkContext *ctx) {
      VkDebugUtilsLabelEXT label;
      label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      label.pNext = nullptr;
      label.pLabelName = labelName.c_str();
      DecodeD3DCOLOR(color, label.color);

      ctx->insertDebugLabel(&label);
    });
  }


  BOOL STDMETHODCALLTYPE D3D9UserDefinedAnnotation::GetStatus() {
    return TRUE;
  }

}
