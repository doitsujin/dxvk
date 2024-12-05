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
            Com<d3d9::IDirect3DStateBlock9>&& pStateBlock)
      : m_device(pDevice)
      , m_stateBlock(std::move(pStateBlock))
      , m_type(Type) {
      if (Type == D3DSBT_VERTEXSTATE || Type == D3DSBT_ALL) {
        // Lights, D3DTSS_TEXCOORDINDEX and D3DTSS_TEXTURETRANSFORMFLAGS,
        // vertex shader, VS constants, and various render states.
        m_capture.vs = true;
      }

      if (Type == D3DSBT_PIXELSTATE || Type == D3DSBT_ALL) {
        // Pixel shader, PS constants, and various RS/TSS states.
        m_capture.ps = true;
      }

      if (Type == D3DSBT_ALL) {
        m_capture.indices = true;
        m_capture.swvp    = true;
        m_capture.textures.setAll();
        m_capture.streams.setAll();
      }

      m_textures.fill(nullptr);
      m_streams.fill(D3D8VBOP());
    }

    ~D3D8StateBlock() {}

    // Construct a state block without a D3D9 object
    D3D8StateBlock(D3D8Device* pDevice)
      : D3D8StateBlock(pDevice, D3DSTATEBLOCKTYPE(0), nullptr) {
    }

    // Attach a D3D9 object to a state block that doesn't have one yet
    void SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock) {
      if (likely(m_stateBlock == nullptr)) {
        m_stateBlock = std::move(pStateBlock);
      } else {
        Logger::err("D3D8StateBlock::SetD3D9: m_stateBlock has already been initialized");
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

  private: // State Data //

    D3D8StateCapture m_capture;

    DWORD m_vertexShader; // vs
    DWORD m_pixelShader;  // ps

    std::array<IDirect3DBaseTexture8*, d8caps::MAX_TEXTURE_STAGES>  m_textures; // textures
    std::array<D3D8VBOP, d8caps::MAX_STREAMS>                       m_streams; // stream data

    IDirect3DIndexBuffer8*  m_indices = nullptr;  // indices
    UINT                    m_baseVertexIndex;    // indices

    bool m_isSWVP;  // D3DRS_SOFTWAREVERTEXPROCESSING
  };


}