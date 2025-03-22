#pragma once

#include "d3d8_caps.h"
#include "d3d8_include.h"
#include "d3d8_device.h"
#include "d3d8_device_child.h"

#include "../util/util_bit.h"

#include <array>

namespace dxvk {

  struct D3D8StateCapture {
    bool vs       : 1;
    bool ps       : 1;
    bool indices  : 1;
    bool swvp     : 1;

    bit::bitset<d8caps::MAX_TEXTURE_STAGES> textures;
    bit::bitset<d8caps::MAX_STREAMS>        streams;

    D3D8StateCapture()
      : vs(false)
      , ps(false)
      , indices(false)
      , swvp(false) {
      // Ensure all bits are initialized to false
      textures.clearAll();
      streams.clearAll();
    }
  };

  // Wrapper class for D3D9 state blocks. Captures D3D8-specific state.
  class D3D8StateBlock  {

  public:

    D3D8StateBlock(
            D3D8Device*                       pDevice,
            D3DSTATEBLOCKTYPE                 Type,
            Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

    D3D8StateBlock(D3D8Device* pDevice);

    void SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

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

    inline HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) {
      m_textures[Stage] = pTexture;
      m_capture.textures.set(Stage, true);
      return D3D_OK;
    }

    inline HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) {
      m_streams[StreamNumber].buffer = pStreamData;
      // The previous stride is preserved if pStreamData is NULL
      if (likely(pStreamData != nullptr))
        m_streams[StreamNumber].stride = Stride;
      m_capture.streams.set(StreamNumber, true);
      return D3D_OK;
    }

    inline HRESULT SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) {
      m_indices         = pIndexData;
      m_baseVertexIndex = BaseVertexIndex;
      m_capture.indices = true;
      return D3D_OK;
    }

    inline HRESULT SetSoftwareVertexProcessing(bool value) {
      m_isSWVP       = value;
      m_capture.swvp = true;
      return D3D_OK;
    }

  private:

    D3D8Device*                     m_device;
    Com<d3d9::IDirect3DStateBlock9> m_stateBlock;
    D3DSTATEBLOCKTYPE               m_type;

    struct D3D8VBOP {
      IDirect3DVertexBuffer8*       buffer = nullptr;
      UINT                          stride = 0;
    };

    // State Data //

    D3D8StateCapture m_capture;

    DWORD m_vertexShader = 0;
    DWORD m_pixelShader  = 0;

    std::array<IDirect3DBaseTexture8*, d8caps::MAX_TEXTURE_STAGES>  m_textures;
    std::array<D3D8VBOP, d8caps::MAX_STREAMS>                       m_streams;

    IDirect3DIndexBuffer8*  m_indices         = nullptr;
    UINT                    m_baseVertexIndex = 0;

    bool m_isSWVP;  // D3DRS_SOFTWAREVERTEXPROCESSING

  };

}