#include "d3d_common_texture.h"

namespace dxvk {

  D3DCommonTexture::D3DCommonTexture(DDrawCommonSurface* commonSurf)
    : m_commonSurf ( commonSurf ) {
  }

  D3DCommonTexture::~D3DCommonTexture() {
    if (m_textureHandle)
      DDrawCommonInterface::ReleaseTextureHandle(m_textureHandle);
  }

}