#include "d3d11_annotation.h"

namespace dxvk {

  D3D11UserDefinedAnnotation::D3D11UserDefinedAnnotation(ID3D11DeviceContext* ctx)
  : m_container(ctx) { }


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
          LPCWSTR                 Name) {
    // Currently not implemented
    return -1;
  }


  INT STDMETHODCALLTYPE D3D11UserDefinedAnnotation::EndEvent() {
    // Currently not implemented
    return -1;
  }


  void STDMETHODCALLTYPE D3D11UserDefinedAnnotation::SetMarker(
          LPCWSTR                 Name) {
    // Currently not implemented
  }


  BOOL STDMETHODCALLTYPE D3D11UserDefinedAnnotation::GetStatus() {
    // Currently not implemented
    return FALSE;
  }

}