#include "d3d3_material.h"

#include "d3d3_interface.h"
#include "d3d3_viewport.h"

#include "../d3d_common_device.h"

#include "../ddraw_common_interface.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  std::atomic<uint32_t> D3D3Material::s_materialCount = 0;

  D3D3Material::D3D3Material(
        D3D3Interface* pParent)
    : DDrawChildObject<D3D3Interface, IDirect3DMaterial>(pParent) {
    m_commonMaterial = new D3DCommonMaterial();

    m_commonMaterial->SetD3D3Material(this);

    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D3Material: Created a new material nr. [[1-", m_materialCount, "]]"));
  }

  D3D3Material::~D3D3Material() {
    m_commonMaterial->SetD3D3Material(nullptr);

    Logger::debug(str::format("D3D3Material: Material nr. [[1-", m_materialCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DMaterial))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Material::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DMaterial object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Material::Initialize(LPDIRECT3D lpDirect3D) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::SetMaterial(D3DMATERIAL *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    material9->Diffuse  = data->dcvDiffuse;
    material9->Ambient  = data->dcvAmbient;
    material9->Specular = data->dcvSpecular;
    material9->Emissive = data->dcvEmissive;
    material9->Power    = data->dvPower;

    m_commonMaterial->DirtyMaterialColor();

    // Update the D3D9 material directly if it's actively being used
    D3DCommonDevice* commonDevice = m_parent->GetCommonInterface()->GetCommonD3DDevice();
    if (likely(commonDevice != nullptr)) {
      const D3DMATERIALHANDLE handle        = m_commonMaterial->GetMaterialHandle();
      const D3DMATERIALHANDLE currentHandle = commonDevice->GetCurrentMaterialHandle();
      if (handle && currentHandle == handle) {
        //Logger::debug(str::format("D3D3Material::SetMaterial: Applying material nr. ", handle, " to D3D9"));
        commonDevice->GetD3D9Device()->SetMaterial(material9);
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetMaterial(D3DMATERIAL *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    data->dcvDiffuse  = material9->Diffuse;
    data->dcvAmbient  = material9->Ambient;
    data->dcvSpecular = material9->Specular;
    data->dcvEmissive = material9->Emissive;
    data->dvPower     = material9->Power;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetHandle(IDirect3DDevice *device, D3DMATERIALHANDLE *handle) {
    if (unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    if(!m_commonMaterial->GetMaterialHandle()) {
      const D3DMATERIALHANDLE nextHandle = D3DCommonInterface::GetNextMaterialHandle();
      m_commonMaterial->SetMaterialHandle(nextHandle);
      D3DCommonInterface::EmplaceMaterial(m_commonMaterial.ptr(), nextHandle);
    }

    *handle = m_commonMaterial->GetMaterialHandle();

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Material::Reserve() {
    return DDERR_UNSUPPORTED;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Material::Unreserve() {
    return DDERR_UNSUPPORTED;
  }

}
