#pragma once

#include <vector>

#include "d3d11_include.h"

namespace dxvk {
  
  class D3D11GDISurface {

  public:

    D3D11GDISurface(
            ID3D11Resource*     pResource,
            UINT                Subresource);

    ~D3D11GDISurface();

    HRESULT Acquire(
            BOOL                Discard,
            HDC*                phdc);

    HRESULT Release(
      const RECT*               pDirtyRect);

  private:

    ID3D11Resource* m_resource;
    uint32_t        m_subresource;
    ID3D11Resource* m_readback;
    HDC             m_hdc;
    HANDLE          m_hbitmap;
    bool            m_acquired;

    std::vector<uint32_t> m_data;

    HRESULT CreateReadbackResource();

  };

}
