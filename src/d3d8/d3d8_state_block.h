#pragma once

#include "d3d8_caps.h"
#include "d3d8_include.h"
#include "d3d8_device.h"
#include "d3d8_device_child.h"

#include "../util/util_bit.h"
#include "../util/util_flags.h"

#include <array>

namespace dxvk {

  enum class D3D8CapturedStateFlag : uint8_t {
    Indices,
    SWVP,
    VertexBuffers,
    Textures,
    VertexShader,
    PixelShader
  };

  using D3D8CapturedStateFlags = Flags<D3D8CapturedStateFlag>;

  struct D3D8StateCaptures {
    D3D8CapturedStateFlags flags;

    bit::bitset<d8caps::MAX_STREAMS>        streams;
    bit::bitset<d8caps::MAX_TEXTURE_STAGES> textures;

    D3D8StateCaptures() {
      // Ensure all bits are initialized to false
      streams.clearAll();
      textures.clearAll();
    }
  };

  struct D3D8VBOP {
    IDirect3DVertexBuffer8* buffer = nullptr;
    UINT                    stride = 0;
  };

  struct D3D8CapturableState {
    std::array<D3D8VBOP, d8caps::MAX_STREAMS>                      streams;
    std::array<IDirect3DBaseTexture8*, d8caps::MAX_TEXTURE_STAGES> textures;

    IDirect3DIndexBuffer8* indices = nullptr;
    UINT  baseVertexIndex    = 0;
    DWORD vertexShaderHandle = 0;
    DWORD pixelShaderHandle  = 0;

    bool isSWVP = false; // D3DRS_SOFTWAREVERTEXPROCESSING
  };

  enum class D3D8StateBlockType : uint8_t {
    None,
    All,
    PixelState,
    VertexState,
    Unknown
  };

  inline D3D8StateBlockType ConvertStateBlockType(D3DSTATEBLOCKTYPE type) {
    switch (type) {
      case D3DSBT_ALL:         return D3D8StateBlockType::All;
      case D3DSBT_PIXELSTATE:  return D3D8StateBlockType::PixelState;
      case D3DSBT_VERTEXSTATE: return D3D8StateBlockType::VertexState;
      default:                 return D3D8StateBlockType::Unknown;
    }
  }

  // Wrapper class for D3D9 state blocks. Captures D3D8-specific state.
  class D3D8StateBlock  {

  public:

    D3D8StateBlock(
            D3D8Device*                       pDevice,
            D3D8StateBlockType                Type,
            Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

    D3D8StateBlock(D3D8Device* pDevice);

    void SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

    HRESULT Capture();

    HRESULT Apply();

    inline HRESULT SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) {
      m_state.indices = pIndexData;
      m_state.baseVertexIndex = BaseVertexIndex;
      m_captures.flags.set(D3D8CapturedStateFlag::Indices);
      return D3D_OK;
    }

    inline HRESULT SetSoftwareVertexProcessing(bool value) {
      m_state.isSWVP = value;
      m_captures.flags.set(D3D8CapturedStateFlag::SWVP);
      return D3D_OK;
    }

    inline HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) {
      m_state.streams[StreamNumber].buffer = pStreamData;
      // The previous stride is preserved if pStreamData is NULL
      if (likely(pStreamData != nullptr))
        m_state.streams[StreamNumber].stride = Stride;
      m_captures.flags.set(D3D8CapturedStateFlag::VertexBuffers);
      m_captures.streams.set(StreamNumber, true);
      return D3D_OK;
    }

    inline HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) {
      m_state.textures[Stage] = pTexture;
      m_captures.flags.set(D3D8CapturedStateFlag::Textures);
      m_captures.textures.set(Stage, true);
      return D3D_OK;
    }

    inline HRESULT SetVertexShader(DWORD Handle) {
      m_state.vertexShaderHandle = Handle;
      m_captures.flags.set(D3D8CapturedStateFlag::VertexShader);
      return D3D_OK;
    }

    inline HRESULT SetPixelShader(DWORD Handle) {
      m_state.pixelShaderHandle = Handle;
      m_captures.flags.set(D3D8CapturedStateFlag::PixelShader);
      return D3D_OK;
    }

  private:

    D3D8Device*                     m_device = nullptr;
    Com<d3d9::IDirect3DStateBlock9> m_stateBlock;

    D3D8CapturableState m_state;
    D3D8StateCaptures   m_captures;

  };

}