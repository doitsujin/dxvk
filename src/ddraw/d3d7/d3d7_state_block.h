#pragma once

#include "../ddraw_include.h"
#include "../ddraw_caps.h"

#include "../util/util_bit.h"
#include "../util/util_flags.h"

#include "../ddraw7/ddraw7_surface.h"

#include <array>

namespace dxvk {

  class D3D7Device;

  enum class D3D7CapturedStateFlag : uint8_t {
    Textures
  };

  using D3D7CapturedStateFlags = Flags<D3D7CapturedStateFlag>;

  struct D3D7StateCaptures {
    D3D7CapturedStateFlags flags;

    bit::bitset<ddrawCaps::TextureStageCount> textures;

    D3D7StateCaptures() {
      // Ensure all bits are initialized to false
      textures.clearAll();
    }
  };

  struct D3D7CapturableState {
    std::array<IDirectDrawSurface7*, ddrawCaps::TextureStageCount> textures;
  };

  enum class D3D7StateBlockType : uint8_t {
    None,
    All,
    PixelState,
    VertexState,
    Unknown
  };

  inline D3D7StateBlockType ConvertStateBlockType(D3DSTATEBLOCKTYPE type) {
    switch (type) {
      case D3DSBT_ALL:         return D3D7StateBlockType::All;
      case D3DSBT_PIXELSTATE:  return D3D7StateBlockType::PixelState;
      case D3DSBT_VERTEXSTATE: return D3D7StateBlockType::VertexState;
      default:                 return D3D7StateBlockType::Unknown;
    }
  }

  // Wrapper class for D3D9 state blocks. Captures D3D7-specific state.
  class D3D7StateBlock  {

  public:

    D3D7StateBlock(
          D3D7Device*                       pDevice,
          D3D7StateBlockType                Type,
          Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

    D3D7StateBlock(D3D7Device* pDevice);

    void SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock);

    HRESULT Capture();

    HRESULT Apply();

    inline HRESULT SetTexture(DWORD Stage, IDirectDrawSurface7* pTexture) {
      m_state.textures[Stage] = pTexture;
      m_captures.flags.set(D3D7CapturedStateFlag::Textures);
      m_captures.textures.set(Stage, true);
      return D3D_OK;
    }

  private:

    D3D7Device*                     m_device = nullptr;

    Com<d3d9::IDirect3DStateBlock9> m_stateBlock;

    D3D7CapturableState             m_state;
    D3D7StateCaptures               m_captures;

  };

}