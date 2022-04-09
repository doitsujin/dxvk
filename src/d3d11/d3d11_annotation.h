#pragma once

#include "d3d11_include.h"
#include "../dxvk/dxvk_annotation.h"

namespace dxvk {

  class D3D11DeviceContext;

  class D3D11UserDefinedAnnotation final : public IDXVKUserDefinedAnnotation {

  public:

    D3D11UserDefinedAnnotation(D3D11DeviceContext* ctx);
    D3D11UserDefinedAnnotation(const D3D11UserDefinedAnnotation&);
    ~D3D11UserDefinedAnnotation();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    INT STDMETHODCALLTYPE BeginEvent(
            D3DCOLOR                Color,
            LPCWSTR                 Name);

    INT STDMETHODCALLTYPE EndEvent();

    void STDMETHODCALLTYPE SetMarker(
            D3DCOLOR                Color,
            LPCWSTR                 Name);

    BOOL STDMETHODCALLTYPE GetStatus();

  private:

    D3D11DeviceContext*  m_container;

    // Stack depth for non-finalized BeginEvent calls
    int32_t m_eventDepth;
  };

}
