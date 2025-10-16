#include "d3d_common_interface.h"

#include "d3d_common_material.h"

namespace dxvk {

  std::atomic<D3DMATERIALHANDLE> D3DCommonInterface::s_materialHandle = 0;

  D3DCommonInterface::D3DCommonInterface()
    : m_d3d9Intf ( d3d9::Direct3DCreate9(D3D_SDK_VERSION) ) {
  }

  D3DCommonInterface::~D3DCommonInterface() {
  }

  D3DCommonMaterial* D3DCommonInterface::GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle) {
    if (unlikely(handle == 0))
      return nullptr;

    auto materialsIter = s_materials.find(handle);
    if (unlikely(materialsIter == s_materials.end())) {
      Logger::warn(str::format("D3DCommonInterface::GetCommonMaterialFromHandle: Unknown handle: ", handle));
      return nullptr;
    }

    return materialsIter->second;
  }

  void D3DCommonInterface::EmplaceMaterial(D3DCommonMaterial* commonMaterial, D3DMATERIALHANDLE handle) {
    s_materials.emplace(std::piecewise_construct,
                        std::forward_as_tuple(handle),
                        std::forward_as_tuple(commonMaterial));
  }

  void D3DCommonInterface::ReleaseMaterialHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = s_materials.find(handle);

    if (likely(materialsIter != s_materials.end()))
      s_materials.erase(materialsIter);
  }

  d3d9::D3DMULTISAMPLE_TYPE D3DCommonInterface::GetMultiSampleType(d3d9::D3DFORMAT backBufferFormat) const {
    HRESULT hr = m_d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                        TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
    if (likely(SUCCEEDED(hr))) {
      Logger::info("D3DCommonInterface::GetMultiSampleType: Using 4x MSAA for FSAA emulation");
      return d3d9::D3DMULTISAMPLE_4_SAMPLES;
    }

    hr = m_d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
    if (SUCCEEDED(hr)) {
      Logger::info("D3DCommonInterface::GetMultiSampleType: Using 2x MSAA for FSAA emulation");
      return d3d9::D3DMULTISAMPLE_2_SAMPLES;
    }

    return d3d9::D3DMULTISAMPLE_NONE;
  }

}