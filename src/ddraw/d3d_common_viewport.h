#pragma once

#include "ddraw_include.h"
#include "ddraw_util.h"

#include "d3d_light.h"

#include <vector>

namespace dxvk {

  class DDrawCommonSurface;
  class D3DCommonDevice;

  class D3D6Viewport;
  class D3D5Viewport;
  class D3D3Viewport;

  class D3D6Device;
  class D3D5Device;
  class D3D3Device;

  class D3DCommonViewport : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonViewport();

    ~D3DCommonViewport();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    D3D6Viewport* GetCurrentD3D6Viewport();

    D3D5Viewport* GetCurrentD3D5Viewport();

    D3D3Viewport* GetCurrentD3D3Viewport();

    DDrawCommonSurface* GetCommonRenderTarget();

    DDrawCommonSurface* GetCommonDepthStencil();

    D3DCommonDevice* GetCommonD3DDevice();

    void UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface);

    HRESULT TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen);

    d3d9::D3DVIEWPORT9* GetD3D9Viewport() {
      return &m_viewport9;
    }

    void MarkViewportAsSet() {
      m_isViewportSet   = true;
      // Also dirty legacy projection on any viewport updates
      m_dirtyProjection = true;
    }

    bool IsViewportSet() const {
      return m_isViewportSet;
    }

    bool IsMaterialSet() const {
      return m_isMaterialSet;
    }

    void SetMaterialHandle(D3DMATERIALHANDLE materialHandle) {
      m_materialHandle = materialHandle;
      m_isMaterialSet = true;
    }

    D3DMATERIALHANDLE GetMaterialHandle() const {
      return m_materialHandle;
    }

    D3DVECTOR* GetLegacyScale() {
      return &m_legacyScale;
    }

    D3DVECTOR* GetLegacyClip() {
      return &m_legacyClip;
    }

    const D3DMATRIX* GetLegacyProjectionMatrix(DWORD drawFlags) {
      // Fast skip if viewport values haven't been set
      if (unlikely(!m_isViewportSet))
        return nullptr;

      const bool needsClipping = !(drawFlags & D3DDP_DONOTCLIP);
      if (unlikely(m_needsClipping != needsClipping)) {
        m_dirtyProjection = true;
        m_needsClipping = needsClipping;
      }

      // Recalculate legacy projection matrix only when needed
      if (unlikely(m_dirtyProjection)) {
        m_legacyProjection._11 = m_legacyScale.x;
        m_legacyProjection._22 = m_legacyScale.y;
        m_legacyProjection._33 = m_legacyScale.z;
        m_legacyProjection._41 = m_needsClipping ? m_legacyClip.x : 0.0f;
        m_legacyProjection._42 = m_needsClipping ? m_legacyClip.y : 0.0f;
        m_legacyProjection._43 = m_needsClipping ? m_legacyClip.z : 0.0f;
        m_legacyProjection._44 = 1.0f;
        // Determine if the projection matrix is an identity matrix
        m_isIdentityMatrix = m_legacyProjection._11 == 1.0f &&
                             m_legacyProjection._22 == 1.0f &&
                             m_legacyProjection._33 == 1.0f &&
                             m_legacyProjection._41 == 0.0f &&
                             m_legacyProjection._42 == 0.0f &&
                             m_legacyProjection._43 == 0.0f;
        m_dirtyProjection = false;
      }

      return m_isIdentityMatrix ? nullptr : &m_legacyProjection;
    }

    void SetIsCurrentViewport(bool isCurrentViewport) {
      m_isCurrentViewport = isCurrentViewport;
    }

    bool IsCurrentViewport() const {
      return m_isCurrentViewport;
    }

    void SetD3D6Viewport(D3D6Viewport* d3d6Viewport) {
      m_d3d6Viewport = d3d6Viewport;
    }

    D3D6Viewport* GetD3D6Viewport() const {
      return m_d3d6Viewport;
    }

    void SetD3D5Viewport(D3D5Viewport* d3d5Viewport) {
      m_d3d5Viewport = d3d5Viewport;
    }

    D3D5Viewport* GetD3D5Viewport() const {
      return m_d3d5Viewport;
    }

    void SetD3D3Viewport(D3D3Viewport* d3d3Viewport) {
      m_d3d3Viewport = d3d3Viewport;
    }

    D3D3Viewport* GetD3D3Viewport() const {
      return m_d3d3Viewport;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

    void SetD3D6Device(D3D6Device* device6) {
      m_device6 = device6;
    }

    D3D6Device* GetD3D6Device() const {
      return m_device6;
    }

    void SetD3D5Device(D3D5Device* device5) {
      m_device5 = device5;
    }

    D3D5Device* GetD3D5Device() const {
      return m_device5;
    }

    void SetD3D3Device(D3D3Device* device3) {
      m_device3 = device3;
    }

    D3D3Device* GetD3D3Device() const {
      return m_device3;
    }

    bool HasDevice() const {
      return m_device6 != nullptr || m_device5 != nullptr || m_device3 != nullptr;
    }

    void GetD3D9Lights(std::vector<d3d9::D3DLIGHT9>* lights9) {
      for (auto light: m_lights) {
        if (light->IsActive())
          lights9->push_back(*light->GetD3D9Light());
      }
    }

    std::vector<Com<D3DLight>>& GetLights() {
      return m_lights;
    }

  private:

    bool                  m_isViewportSet     = false;
    bool                  m_isCurrentViewport = false;
    bool                  m_isMaterialSet     = false;

    // Legacy projection state
    bool                  m_isIdentityMatrix  = false;
    bool                  m_needsClipping     = false;
    bool                  m_dirtyProjection   = false;

    D3DMATERIALHANDLE     m_materialHandle    = 0;

    D3DVECTOR             m_legacyScale       = { };
    D3DVECTOR             m_legacyClip        = { };
    D3DMATRIX             m_legacyProjection  = { };

    d3d9::D3DVIEWPORT9    m_viewport9         = { };

    D3D6Viewport*         m_d3d6Viewport      = nullptr;
    D3D5Viewport*         m_d3d5Viewport      = nullptr;
    D3D3Viewport*         m_d3d3Viewport      = nullptr;

    // Track the origin viewport, as in the viewport
    // that gets created through a CreateViewport call
    IUnknown*             m_origin            = nullptr;

    // Track all devices this viewport is attached to
    D3D6Device*           m_device6           = nullptr;
    D3D5Device*           m_device5           = nullptr;
    D3D3Device*           m_device3           = nullptr;

    std::vector<Com<D3DLight>> m_lights;

  };

}