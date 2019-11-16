#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_gdi.h"

#ifndef DXVK_NATIVE
#include "../util/util_gdi.h"
#endif

namespace dxvk {
  
  D3D11GDISurface::D3D11GDISurface(
          ID3D11Resource*     pResource,
          UINT                Subresource)
  : m_resource    (pResource),
    m_subresource (Subresource),
    m_readback    (nullptr),
    m_hdc         (nullptr),
    m_hbitmap     (nullptr),
    m_acquired    (false) {
    // Allocate memory for the bitmap
    auto tex = GetCommonTexture(m_resource)->Desc();
    m_data.resize(tex->Width * tex->Height);

    // Create GDI DC
    D3DKMT_CREATEDCFROMMEMORY desc;
    desc.pMemory     = m_data.data();
    desc.Format      = D3DFMT_A8R8G8B8;
    desc.Width       = tex->Width;
    desc.Height      = tex->Height;
    desc.Pitch       = tex->Width * sizeof(uint32_t);
    desc.hDeviceDc   = CreateCompatibleDC(nullptr);
    desc.pColorTable = nullptr;
    desc.hDc         = nullptr;
    desc.hBitmap     = nullptr;

    if (D3DKMTCreateDCFromMemory(&desc))
      Logger::err(str::format("D3D11: Failed to create GDI DC"));
    
    m_hdc     = desc.hDc;
    m_hbitmap = desc.hBitmap;
  }


  D3D11GDISurface::~D3D11GDISurface() {
    if (m_readback)
      m_readback->Release();

    D3DKMT_DESTROYDCFROMMEMORY desc;
    desc.hDC     = m_hdc;
    desc.hBitmap = m_hbitmap;
    D3DKMTDestroyDCFromMemory(&desc);
  }

  
  HRESULT D3D11GDISurface::Acquire(BOOL Discard, HDC* phdc) {
    if (!phdc)
      return E_INVALIDARG;
    
    *phdc = nullptr;

    if (m_acquired)
      return DXGI_ERROR_INVALID_CALL;
    
    if (!Discard) {
      // Create a staging resource that we can map
      if (!m_readback && FAILED(CreateReadbackResource())) {
        Logger::err("D3D11: Failed to create GDI readback resource");
        return E_FAIL;
      }

      // Copy subresource to staging image
      Com<ID3D11Device>         device;
      Com<ID3D11DeviceContext>  context;

      m_resource->GetDevice(&device);
      device->GetImmediateContext(&context);

      context->CopySubresourceRegion(m_readback, 0,
        0, 0, 0, m_resource, m_subresource, nullptr);

      // Copy staging image to DC memory
      auto tex       = GetCommonTexture(m_resource)->Desc();
      auto rowData   = reinterpret_cast<char*>(m_data.data());
      auto rowLength = sizeof(uint32_t) * tex->Width;

      D3D11_MAPPED_SUBRESOURCE sr;
      context->Map(m_readback, 0, D3D11_MAP_READ, 0, &sr);

      for (uint32_t i = 0; i < tex->Height; i++) {
        std::memcpy(rowData + rowLength * i,
          reinterpret_cast<const char*>(sr.pData) + sr.RowPitch * i,
          rowLength);
      }

      context->Unmap(m_readback, 0);
    }
    
    m_acquired = true;
    *phdc      = m_hdc;
    return S_OK;
  }

  
  HRESULT D3D11GDISurface::Release(const RECT* pDirtyRect) {
    if (!m_acquired)
      return DXGI_ERROR_INVALID_CALL;

    Com<ID3D11Device>         device;
    Com<ID3D11DeviceContext>  context;

    m_resource->GetDevice(&device);
    device->GetImmediateContext(&context);

    // Commit changes made to the DC
    auto tex = GetCommonTexture(m_resource)->Desc();

    RECT rect;

    if (pDirtyRect) {
      rect.left   = std::max<LONG>(pDirtyRect->left,   0);
      rect.top    = std::max<LONG>(pDirtyRect->top,    0);
      rect.right  = std::min<LONG>(pDirtyRect->right,  tex->Width);
      rect.bottom = std::min<LONG>(pDirtyRect->bottom, tex->Height);
    } else {
      rect.left   = 0;
      rect.top    = 0;
      rect.right  = tex->Width;
      rect.bottom = tex->Height;
    }

    if (rect.left < rect.right && rect.top < rect.bottom) {
      D3D11_BOX box;
      box.left    = rect.left;
      box.top     = rect.top;
      box.front   = 0;
      box.right   = rect.right;
      box.bottom  = rect.bottom;
      box.back    = 1;

      context->UpdateSubresource(m_resource, m_subresource,
        &box, m_data.data() + rect.left,
        sizeof(uint32_t) * tex->Width,
        sizeof(uint32_t) * tex->Width * tex->Height);
    }
    
    m_acquired = false;
    return S_OK;
  }


  HRESULT D3D11GDISurface::CreateReadbackResource() {
    auto tex = GetCommonTexture(m_resource);

    Com<ID3D11Device>         device;
    Com<ID3D11DeviceContext>  context;

    m_resource->GetDevice(&device);
    device->GetImmediateContext(&context);

    D3D11_RESOURCE_DIMENSION dim = { };
    m_resource->GetType(&dim);

    VkImageSubresource sr = tex->GetSubresourceFromIndex(
      VK_IMAGE_ASPECT_COLOR_BIT, m_subresource);

    switch (dim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC desc;
        desc.Width     = std::max<UINT>(tex->Desc()->Width >> sr.mipLevel, 1);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format    = tex->Desc()->Format;
        desc.Usage     = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        
        ID3D11Texture1D* tex1D = nullptr;
        HRESULT hr = device->CreateTexture1D(&desc, nullptr, &tex1D);
        m_readback = tex1D;
        return hr;
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width     = std::max<UINT>(tex->Desc()->Width  >> sr.mipLevel, 1);
        desc.Height    = std::max<UINT>(tex->Desc()->Height >> sr.mipLevel, 1);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format    = tex->Desc()->Format;
        desc.SampleDesc= { 1, 0 };
        desc.Usage     = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        
        ID3D11Texture2D* tex2D = nullptr;
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &tex2D);
        m_readback = tex2D;
        return hr;
      } break;

      default:
        return E_INVALIDARG;
    }
  }

}
