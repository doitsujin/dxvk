#pragma once

#include <type_traits>

#include "d3d11_include.h"

#include "../dxvk/dxvk_annotation.h"
#include "../dxvk/dxvk_device.h"

namespace dxvk {

  class D3D11DeferredContext;
  class D3D11ImmediateContext;

  template<typename ContextType>
  class D3D11UserDefinedAnnotation final : public IDXVKUserDefinedAnnotation {
    constexpr static bool IsDeferred = std::is_same_v<ContextType, D3D11DeferredContext>;
  public:

    D3D11UserDefinedAnnotation(
            ContextType*          container,
      const Rc<DxvkDevice>&       dxvkDevice);

    ~D3D11UserDefinedAnnotation();

    D3D11UserDefinedAnnotation             (const D3D11UserDefinedAnnotation&) = delete;
    D3D11UserDefinedAnnotation& operator = (const D3D11UserDefinedAnnotation&) = delete;

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

    ContextType*  m_container           = nullptr;
    int32_t       m_eventDepth          = 0u;
    bool          m_annotationsEnabled  = false;

  };

}
