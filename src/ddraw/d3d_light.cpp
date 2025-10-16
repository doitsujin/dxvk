#include "d3d_light.h"

#include "d3d3/d3d3_viewport.h"
#include "d3d5/d3d5_viewport.h"
#include "d3d6/d3d6_viewport.h"

namespace dxvk {

  std::atomic<uint32_t> D3DLight::s_lightCount = 0;

  D3DLight::D3DLight(IUnknown* pParent)
    : DDrawChildObject<IUnknown, IDirect3DLight>(pParent) {
    m_lightCount = ++s_lightCount;

    Logger::debug(str::format("D3DLight: Created a new light nr. [[1-", m_lightCount, "]]"));
  }

  D3DLight::~D3DLight() {
    Logger::debug(str::format("D3DLight: Light nr. [[1-", m_lightCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3DLight::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DLight))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3DLight::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "The method returns DDERR_ALREADYINITIALIZED because
  // the Direct3DLight object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3DLight::Initialize(IDirect3D *d3d) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3DLight::SetLight(D3DLIGHT *data) {
    static constexpr D3DCOLORVALUE noLight = {0.0f, 0.0f, 0.0f, 0.0f};

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dltType || data->dltType > D3DLIGHT_GLSPOT))
      return DDERR_INVALIDPARAMS;

    // Hidden & Dangeous spams a lot of parallel point lights
    if (unlikely(data->dltType == D3DLIGHT_PARALLELPOINT)) {
      static bool s_parallelPointEmulationShown;

      if (!std::exchange(s_parallelPointEmulationShown, true))
        Logger::info("D3DLight::SetLight: Using D3DLIGHT_PARALLELPOINT emulation");

      m_isParallelPoint = true;
    } else {
      m_isParallelPoint = false;
    }
    // Specific to D3D3
    if (unlikely(data->dltType == D3DLIGHT_GLSPOT)) {
      static bool s_glSpotErrorShown;

      if (!std::exchange(s_glSpotErrorShown, true))
        Logger::warn("D3DLight::SetLight: Unsupported light type D3DLIGHT_GLSPOT");

      return DDERR_INVALIDPARAMS;
    }

    // Docs: "Although this method's declaration specifies the lpLight parameter as being
    // the address of a D3DLIGHT structure, that structure is not normally used. Rather,
    // the D3DLIGHT2 structure is recommended to achieve the best lighting effects."
    const bool isD3DLight2 = data->dwSize == sizeof(D3DLIGHT2);
    m_flags                = isD3DLight2 ? reinterpret_cast<D3DLIGHT2*>(data)->dwFlags : 0;
    const bool hasSpecular = isD3DLight2 ? !(m_flags & D3DLIGHT_NO_SPECULAR) : true;

    // Emulate parallel point lights with point lights of maximum range and no attenuation
    m_light9.Type         = !m_isParallelPoint ? d3d9::D3DLIGHTTYPE(data->dltType) : d3d9::D3DLIGHT_POINT;
    m_light9.Diffuse      = data->dcvColor;
    m_light9.Specular     = hasSpecular ? data->dcvColor : noLight;
    // Ambient light always comes from the material
    //m_light9.Ambient    = noLight;
    m_light9.Position     = data->dvPosition;
    m_light9.Direction    = data->dvDirection;
    // "Parallel-point lights have only color and position. They create lighting effects
    //  similar to point lights but with less overhead, at the cost of some accuracy."
    // "Like directional lights, parallel-point lights are not affected by range or attenuation."
    m_light9.Range        = !m_isParallelPoint ? data->dvRange : D3DLIGHT_RANGE_MAX;
    m_light9.Falloff      = data->dvFalloff;
    m_light9.Attenuation0 = !m_isParallelPoint ? data->dvAttenuation0 : 1.0f;
    m_light9.Attenuation1 = !m_isParallelPoint ? data->dvAttenuation1 : 0.0f;
    m_light9.Attenuation2 = !m_isParallelPoint ? data->dvAttenuation2 : 0.0f;
    m_light9.Theta        = data->dvTheta;
    m_light9.Phi          = data->dvPhi;
    // D3DLIGHT structure lights are, apparently, considered to be active by default
    m_isActive            = isD3DLight2 ? (m_flags & D3DLIGHT_ACTIVE) : true;

    // Update the D3D9 light directly if it's actively being used
    if (m_viewport6 != nullptr && m_viewport6->GetCommonViewport()->IsCurrentViewport())
      m_viewport6->ApplyAndActivateLight(m_lightCount, this);
    else if (m_viewport5 != nullptr && m_viewport5->GetCommonViewport()->IsCurrentViewport())
      m_viewport5->ApplyAndActivateLight(m_lightCount, this);
    else if (m_viewport3 != nullptr && m_viewport3->GetCommonViewport()->IsCurrentViewport())
      m_viewport3->ApplyAndActivateLight(m_lightCount, this);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DLight::GetLight(D3DLIGHT *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    data->dltType         = !m_isParallelPoint ? D3DLIGHTTYPE(m_light9.Type) : D3DLIGHT_PARALLELPOINT;
    data->dcvColor        = m_light9.Diffuse;
    //data->dcvColor      = m_light9.Specular;
    data->dvPosition      = m_light9.Position;
    data->dvDirection     = m_light9.Direction;
    data->dvRange         = !m_isParallelPoint ? m_light9.Range : 0.0f;
    data->dvFalloff       = m_light9.Falloff;
    data->dvAttenuation0  = !m_isParallelPoint ? m_light9.Attenuation0 : 0.0f;
    data->dvAttenuation1  = !m_isParallelPoint ? m_light9.Attenuation1 : 0.0f;
    data->dvAttenuation2  = !m_isParallelPoint ? m_light9.Attenuation2 : 0.0f;
    data->dvTheta         = m_light9.Theta;
    data->dvPhi           = m_light9.Phi;

    if (data->dwSize == sizeof(D3DLIGHT2))
      reinterpret_cast<D3DLIGHT2*>(data)->dwFlags = m_flags;

    return D3D_OK;
  }

}
