#include "d3d7_state_block.h"

#include "d3d7_device.h"

namespace dxvk {

  D3D7StateBlock::D3D7StateBlock(
        D3D7Device*                       pDevice,
        D3D7StateBlockType                Type,
        Com<d3d9::IDirect3DStateBlock9>&& pStateBlock)
    : m_device     ( pDevice )
    , m_stateBlock ( std::move(pStateBlock) ) {
    if (Type == D3D7StateBlockType::All) {
      m_captures.flags.set(D3D7CapturedStateFlag::Textures);
      m_captures.textures.setAll();
    }

    m_state.textures.fill(nullptr);

    // Automatically capture state on creation via D3D7Device::CreateStateBlock.
    if (Type != D3D7StateBlockType::None)
      Capture();
  }

  // Construct a state block without a D3D9 object
  D3D7StateBlock::D3D7StateBlock(D3D7Device* pDevice)
    : D3D7StateBlock(pDevice, D3D7StateBlockType::None, nullptr) {
  }

  // Attach a D3D9 object to a state block that doesn't have one yet
  void D3D7StateBlock::SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock) {
    if (likely(m_stateBlock == nullptr)) {
      m_stateBlock = std::move(pStateBlock);
    } else {
      Logger::err("D3D7StateBlock::SetD3D9: m_stateBlock has already been initialized");
    }
  }

  HRESULT D3D7StateBlock::Capture() {
    if (unlikely(m_stateBlock == nullptr))
      return D3DERR_INVALIDCALL;

    if (m_captures.flags.test(D3D7CapturedStateFlag::Textures)) {
      for (DWORD stage = 0; stage < m_state.textures.size(); stage++) {
        if (m_captures.textures.get(stage))
          m_state.textures[stage] = m_device->m_textures[stage].ptr();
      }
    }

    return m_stateBlock->Capture();
  }

  HRESULT D3D7StateBlock::Apply() {
    if (unlikely(m_stateBlock == nullptr))
      return D3DERR_INVALIDCALL;

    HRESULT res = m_stateBlock->Apply();

    if (m_captures.flags.test(D3D7CapturedStateFlag::Textures)) {
      for (DWORD stage = 0; stage < m_state.textures.size(); stage++) {
        if (m_captures.textures.get(stage))
          m_device->SetTexture(stage, m_state.textures[stage]);
      }
    }

    return res;
  }

}
