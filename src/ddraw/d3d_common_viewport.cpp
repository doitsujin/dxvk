#include "d3d_common_viewport.h"

#include "ddraw_common_surface.h"

#include "d3d_common_device.h"

#include "ddraw4/ddraw4_surface.h"
#include "ddraw/ddraw_surface.h"

#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonViewport::D3DCommonViewport() {
  }

  D3DCommonViewport::~D3DCommonViewport() {
  }

  D3D6Viewport* D3DCommonViewport::GetCurrentD3D6Viewport() {
    if (likely(m_device6 != nullptr))
      return m_device6->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D5Viewport* D3DCommonViewport::GetCurrentD3D5Viewport() {
    if (likely(m_device5 != nullptr))
      return m_device5->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D3Viewport* D3DCommonViewport::GetCurrentD3D3Viewport() {
    if (likely(m_device3 != nullptr))
      return m_device3->GetCurrentViewportInternal();

    return nullptr;
  }

  DDrawCommonSurface* D3DCommonViewport::GetCommonRenderTarget() {
    if (m_device6 != nullptr) {
      DDraw4Surface* rt = m_device6->GetRenderTarget();
      if (likely(rt != nullptr))
        return rt->GetCommonSurface();
    }
    if (m_device5 != nullptr) {
      DDrawSurface* rt = m_device5->GetRenderTarget();
      if (likely(rt != nullptr))
        return rt->GetCommonSurface();
    }
    if (m_device3 != nullptr) {
      DDrawSurface* rt = m_device3->GetRenderTarget();
      if (likely(rt != nullptr))
        return rt->GetCommonSurface();
    }

    return nullptr;
  }

  DDrawCommonSurface* D3DCommonViewport::GetCommonDepthStencil() {
    if (m_device6 != nullptr) {
      DDraw4Surface* ds = m_device6->GetDepthStencil();
      if (likely(ds != nullptr))
        return ds->GetCommonSurface();
    }
    if (m_device5 != nullptr) {
      DDrawSurface* ds = m_device5->GetDepthStencil();
      if (likely(ds != nullptr))
        return ds->GetCommonSurface();
    }
    if (m_device3 != nullptr) {
      DDrawSurface* ds = m_device3->GetDepthStencil();
      if (likely(ds != nullptr))
        return ds->GetCommonSurface();
    }

    return nullptr;
  }

  D3DCommonDevice* D3DCommonViewport::GetCommonD3DDevice() {
    if (m_device6 != nullptr) {
      return m_device6->GetCommonD3DDevice();
    } else if (m_device5 != nullptr) {
      return m_device5->GetCommonD3DDevice();
    } else if (m_device3 != nullptr) {
      return m_device3->GetCommonD3DDevice();
    }

    return nullptr;
  }

  void D3DCommonViewport::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if (m_device6 != nullptr) {
      m_device6->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device5 != nullptr) {
      m_device5->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device3 != nullptr) {
      m_device3->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    }
  }

  HRESULT D3DCommonViewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    if (unlikely(data == nullptr || offscreen == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DTRANSFORMDATA)))
      return DDERR_INVALIDPARAMS;

    if (unlikely((flags & (D3DTRANSFORM_CLIPPED | D3DTRANSFORM_UNCLIPPED)) == 0))
      return DDERR_INVALIDPARAMS;

    const bool clipped = (flags & D3DTRANSFORM_CLIPPED) && !(flags & D3DTRANSFORM_UNCLIPPED);

    if (clipped)
      *offscreen = UINT_MAX;
    else
      *offscreen = 0;

    // When vertex_count = 0 native apparently returns success even when data->lpIn/data->lpOut are null, otherwise crash
    if (unlikely(vertex_count == 0))
      return D3D_OK;

    if (unlikely(data->dwInSize < sizeof(D3DLVERTEX) || data->dwOutSize < sizeof(D3DTLVERTEX)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->lpIn == nullptr || data->lpOut == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ensure transform states aren't modified in flight
    D3DDeviceLock lock6, lock5, lock3;
    if (m_device6 != nullptr)
      lock6 = m_device6->LockDevice();
    if (m_device5 != nullptr)
      lock5 = m_device5->LockDevice();
    if (m_device3 != nullptr)
      lock3 = m_device3->LockDevice();

    d3d9::IDirect3DDevice9* m_device9 = GetCommonD3DDevice()->GetD3D9Device();

    D3DMATRIX world9, view9, projection9;
    HRESULT hr;
    hr = m_device9->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world9);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get D3D9 world transform");
      return DDERR_GENERIC;
    }
    hr = m_device9->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_VIEW), &view9);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get D3D9 view transform");
      return DDERR_GENERIC;
    }
    hr = m_device9->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_PROJECTION), &projection9);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get D3D9 projection transform");
      return DDERR_GENERIC;
    }

    // Precalculate a few static viewport factors, to save on per-vertex cycles
    const float viewport9HalfWidth  = static_cast<float>(m_viewport9.Width)  * 0.5f;
    const float viewport9HalfHeight = static_cast<float>(m_viewport9.Height) * 0.5f;
    const float viewport9ZDelta     = m_viewport9.MaxZ - m_viewport9.MinZ;

    const D3DMATRIX* correction = GetLegacyProjectionMatrix(0);

    const Matrix4 wv = MatrixD3DTo4(&view9) * MatrixD3DTo4(&world9);
    const Matrix4 wvp = correction == nullptr ? MatrixD3DTo4(&projection9) * wv
                                              : MatrixD3DTo4(correction) * MatrixD3DTo4(&projection9) * wv;

    for (DWORD t = 0; t < vertex_count; t++) {
      // Docs says input is always D3DLVERTEX and output D3DTLVERTEX.
      // But they can have arbitrary stride set by application and defined via dwInSize/dwOutSize.
      D3DLVERTEX& in = *(reinterpret_cast<D3DLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpIn) + data->dwInSize * t));
      D3DTLVERTEX& out = *(reinterpret_cast<D3DTLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpOut) + data->dwOutSize * t));

      const Vector4 h = wvp * Vector4({in.x, in.y, in.z, 1.0f});

      auto outH = data->lpHOut;
      if (outH != nullptr && clipped) {
        outH[t].dwFlags = 0;
        if (h.x > h.w)
          outH[t].dwFlags |= D3DCLIP_RIGHT;
        if (h.x < -h.w)
          outH[t].dwFlags |= D3DCLIP_LEFT;
        if (h.y > h.w)
          outH[t].dwFlags |= D3DCLIP_TOP;
        if (h.y < -h.w)
          outH[t].dwFlags |= D3DCLIP_BOTTOM;
        if (h.z < 0.0f)
          outH[t].dwFlags |= D3DCLIP_FRONT;
        if (h.z > h.w)
          outH[t].dwFlags |= D3DCLIP_BACK;

        *offscreen &= outH[t].dwFlags;

        outH[t].hx = (h.x - m_legacyClip.x * h.w) / m_legacyScale.x;
        outH[t].hy = (h.y - m_legacyClip.y * h.w) / m_legacyScale.y;
        outH[t].hz = (h.z - m_legacyClip.z * h.w) / m_legacyScale.z;

        if (outH[t].dwFlags) {
          out.sx = h.x;
          out.sy = h.y;
          out.sz = h.z;
          out.rhw = h.w;
          continue;
        }
      }

      // Hidden & Dangerous (D3D6) relies on NAN/INF output
      // in ProcessVertices, so do the same here just in case
      out.rhw = 1.0f / h.w;
      out.sx = m_viewport9.X + viewport9HalfWidth * (h.x * out.rhw + 1.0f);
      out.sy = m_viewport9.Y + viewport9HalfHeight * (1.0f - h.y * out.rhw);
      out.sz = m_viewport9.MinZ + h.z * out.rhw * viewport9ZDelta;

      out.color = in.color;
      out.specular = in.specular;
      out.tu = in.tu;
      out.tv = in.tv;
    }

    return D3D_OK;
  }

}