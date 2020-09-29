#pragma once

#include "d3d8_include.h"
#include "d3d8_device.h"
#include "d3d8_device_child.h"

namespace dxvk {

  struct D3D8StateCapture {
    bool vs : 1;
    bool ps : 1;
  };

  class D3D8StateBlock  {

  public:

    D3D8StateBlock(
            D3D8DeviceEx* pDevice,
            Com<d3d9::IDirect3DStateBlock9>&& pStateBlock)
      : m_device(pDevice)
      , m_stateBlock(std::move(pStateBlock)) {
    }

    ~D3D8StateBlock() {}

    // Construct a state block without a D3D9 object
    D3D8StateBlock(D3D8DeviceEx* pDevice)
      : D3D8StateBlock(pDevice, nullptr) {
    }

    void SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock) {
      if (likely(m_stateBlock == nullptr)) {
        m_stateBlock = std::move(pStateBlock);
      } else {
        Logger::err("D3D8StateBlock::SetD3D9 called when m_stateBlock has already been initialized");
      }
    }

    HRESULT Capture();

    HRESULT Apply();

    inline HRESULT SetVertexShader(DWORD Handle) {
      m_vertexShader  = Handle;
      m_capture.vs    = true;
      return D3D_OK;
    }

    inline HRESULT SetPixelShader(DWORD Handle) {
      m_pixelShader = Handle;
      m_capture.ps  = true;
      return D3D_OK;
    }

  private:
    D3D8DeviceEx*                   m_device;
    Com<d3d9::IDirect3DStateBlock9> m_stateBlock;

    // State Data //

    D3D8StateCapture m_capture;

    DWORD m_vertexShader;
    DWORD m_pixelShader;
  };


}