#include "d3d_common_material.h"

namespace dxvk {

  D3DCommonMaterial::D3DCommonMaterial() {
  }

  D3DCommonMaterial::~D3DCommonMaterial() {
    if (m_materialHandle)
      D3DCommonInterface::ReleaseMaterialHandle(m_materialHandle);
  }

}