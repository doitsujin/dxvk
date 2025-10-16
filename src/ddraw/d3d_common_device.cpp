#include "d3d_common_device.h"

#include "ddraw_common_surface.h"

#include "d3d_common_interface.h"

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw7/ddraw7_surface.h"

#include "d3d7/d3d7_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonDevice::D3DCommonDevice(
        DDrawCommonInterface* commonIntf,
        GUID deviceGUID,
        const d3d9::D3DPRESENT_PARAMETERS* pParams9,
        DWORD creationFlags9)
    : m_commonIntf     ( commonIntf )
    , m_deviceGUID     ( deviceGUID )
    , m_params9        ( *pParams9 )
    , m_creationFlags9 ( creationFlags9 ) {
  }

  D3DCommonDevice::~D3DCommonDevice() {
    if (m_commonIntf->GetCommonD3DDevice() == this)
      m_commonIntf->SetCommonD3DDevice(nullptr);
  }

  D3DCommonInterface* D3DCommonDevice::GetCommonD3DInterface() const {
    if (m_device7 != nullptr) {
      return m_device7->GetParent() != nullptr ? m_device7->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device6 != nullptr) {
      return m_device6->GetParent() != nullptr ? m_device6->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device5 != nullptr) {
      return m_device5->GetParent() != nullptr ? m_device5->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device3 != nullptr) {
      D3D3Interface* d3d3Intf = m_device3->GetParent()->GetCommonInterface()->GetD3D3Interface();
      return d3d3Intf != nullptr ? d3d3Intf->GetCommonD3DInterface() : nullptr;
    }

    return nullptr;
  }

  HRESULT D3DCommonDevice::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    if (m_device7 != nullptr) {
      return m_device7->ResetD3D9Swapchain(params);
    } else if (m_device6 != nullptr) {
      return m_device6->ResetD3D9Swapchain(params);
    }
    // D3D5/3 has no way of disabling/re-enabling VSync

    return DDERR_UNSUPPORTED;
  }

  DDrawSurface* D3DCommonDevice::GetCurrentRenderTarget() const {
    return m_device5 != nullptr ? m_device5->GetRenderTarget() :
           m_device3 != nullptr ? m_device3->GetRenderTarget() : nullptr;
  }

  DDraw4Surface* D3DCommonDevice::GetCurrentRenderTarget4() const {
    return m_device6 != nullptr ? m_device6->GetRenderTarget() : nullptr;
  }

  DDraw7Surface* D3DCommonDevice::GetCurrentRenderTarget7() const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget() : nullptr;
  }

  bool D3DCommonDevice::IsCurrentRenderTarget(DDrawCommonSurface* commonSurface) const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device6 != nullptr ? m_device6->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device5 != nullptr ? m_device5->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device3 != nullptr ? m_device3->GetRenderTarget()->GetCommonSurface() == commonSurface : false;
  }

}