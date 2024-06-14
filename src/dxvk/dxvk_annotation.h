#pragma once

#include <d3d11_1.h>
#include <d3d9.h>

MIDL_INTERFACE("7f2c2f72-1cc8-4979-8d9c-7e3faeddecde")
IDXVKUserDefinedAnnotation : public ID3DUserDefinedAnnotation {

public:

  INT STDMETHODCALLTYPE BeginEvent(
          LPCWSTR                 Name) final {
    return this->BeginEvent(0, Name);
  }

  void STDMETHODCALLTYPE SetMarker(
          LPCWSTR                 Name) final {
    this->SetMarker(0, Name);
  }

  virtual INT STDMETHODCALLTYPE BeginEvent(
          D3DCOLOR                Color,
          LPCWSTR                 Name) = 0;

  virtual void STDMETHODCALLTYPE SetMarker(
          D3DCOLOR                Color,
          LPCWSTR                 Name) = 0;

};

#ifndef _MSC_VER
__CRT_UUID_DECL(IDXVKUserDefinedAnnotation, 0x7f2c2f72,0x1cc8,0x4979,0x8d,0x9c,0x7e,0x3f,0xae,0xdd,0xec,0xde);
#endif
