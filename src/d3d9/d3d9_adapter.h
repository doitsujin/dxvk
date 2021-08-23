#pragma once

#include "d3d9_include.h"

#include "d3d9_options.h"
#include "d3d9_format.h"

#include "../dxvk/dxvk_adapter.h"

namespace dxvk {

  class D3D9InterfaceEx;

  class D3D9Adapter {

  public:

    D3D9Adapter(
            D3D9InterfaceEx* pParent,
            Rc<DxvkAdapter>  Adapter,
            UINT             Ordinal,
            UINT             DisplayIndex);

    HRESULT GetAdapterIdentifier(
            DWORD                   Flags,
            D3DADAPTER_IDENTIFIER9* pIdentifier);

    HRESULT CheckDeviceType(
            D3DDEVTYPE DevType,
            D3D9Format AdapterFormat,
            D3D9Format BackBufferFormat,
            BOOL       bWindowed);

    HRESULT CheckDeviceFormat(
            D3DDEVTYPE      DeviceType,
            D3D9Format      AdapterFormat,
            DWORD           Usage,
            D3DRESOURCETYPE RType,
            D3D9Format      CheckFormat);

    HRESULT CheckDeviceMultiSampleType(
            D3DDEVTYPE          DeviceType,
            D3D9Format          SurfaceFormat,
            BOOL                Windowed,
            D3DMULTISAMPLE_TYPE MultiSampleType,
            DWORD*              pQualityLevels);

    HRESULT CheckDepthStencilMatch(
            D3DDEVTYPE DeviceType,
            D3D9Format AdapterFormat,
            D3D9Format RenderTargetFormat,
            D3D9Format DepthStencilFormat);

    HRESULT CheckDeviceFormatConversion(
            D3DDEVTYPE DeviceType,
            D3D9Format SourceFormat,
            D3D9Format TargetFormat);

    HRESULT GetDeviceCaps(
            D3DDEVTYPE DeviceType,
            D3DCAPS9*  pCaps);

    HMONITOR GetMonitor();

    UINT GetAdapterModeCountEx(const D3DDISPLAYMODEFILTER* pFilter);

    HRESULT EnumAdapterModesEx(
      const D3DDISPLAYMODEFILTER* pFilter,
            UINT                  Mode,
            D3DDISPLAYMODEEX*     pMode);

    HRESULT GetAdapterDisplayModeEx(
            D3DDISPLAYMODEEX*   pMode,
            D3DDISPLAYROTATION* pRotation);

    HRESULT GetAdapterLUID(LUID* pLUID);

    UINT GetOrdinal() { return m_ordinal; }

    Rc<DxvkAdapter> GetDXVKAdapter() { return m_adapter; }

    D3D9_VK_FORMAT_MAPPING GetFormatMapping(D3D9Format Format) const {
      return m_d3d9Formats.GetFormatMapping(Format);
    }

    const DxvkFormatInfo* GetUnsupportedFormatInfo(D3D9Format Format) const {
      return m_d3d9Formats.GetUnsupportedFormatInfo(Format);
    }

  private:

    HRESULT CheckDeviceVkFormat(
          VkFormat        Format,
          DWORD           Usage,
          D3DRESOURCETYPE RType);

    void CacheModes(D3D9Format Format);

    D3D9InterfaceEx*              m_parent;

    Rc<DxvkAdapter>               m_adapter;
    UINT                          m_ordinal;
    UINT                          m_displayIndex;

    std::vector<D3DDISPLAYMODEEX> m_modes;
    D3D9Format                    m_modeCacheFormat;

    const D3D9VkFormatTable       m_d3d9Formats;

  };

}