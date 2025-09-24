#include "d3d8_device.h"
#include "d3d8_state_block.h"

namespace dxvk {

  D3D8StateBlock::D3D8StateBlock(
          D3D8Device*                       pDevice,
          D3D8StateBlockType                Type,
          Com<d3d9::IDirect3DStateBlock9>&& pStateBlock)
    : m_device(pDevice)
    , m_stateBlock(std::move(pStateBlock)) {
    if (Type == D3D8StateBlockType::All) {
      m_captures.flags.set(D3D8CapturedStateFlag::Indices);
      m_captures.flags.set(D3D8CapturedStateFlag::SWVP);

      m_captures.flags.set(D3D8CapturedStateFlag::VertexBuffers);
      m_captures.streams.setAll();

      m_captures.flags.set(D3D8CapturedStateFlag::Textures);
      m_captures.textures.setAll();
    }

    if (Type == D3D8StateBlockType::VertexState || Type == D3D8StateBlockType::All) {
      // Lights, D3DTSS_TEXCOORDINDEX and D3DTSS_TEXTURETRANSFORMFLAGS,
      // vertex shader, VS constants, and various render states.
      m_captures.flags.set(D3D8CapturedStateFlag::VertexShader);
    }

    if (Type == D3D8StateBlockType::PixelState || Type == D3D8StateBlockType::All) {
      // Pixel shader, PS constants, and various RS/TSS states.
      m_captures.flags.set(D3D8CapturedStateFlag::PixelShader);
    }

    m_state.textures.fill(nullptr);
    m_state.streams.fill(D3D8VBOP());

    // Automatically capture state on creation via D3D8Device::CreateStateBlock.
    if (Type != D3D8StateBlockType::None)
      Capture();
  }

  // Construct a state block without a D3D9 object
  D3D8StateBlock::D3D8StateBlock(D3D8Device* pDevice)
    : D3D8StateBlock(pDevice, D3D8StateBlockType::None, nullptr) {
  }

  // Attach a D3D9 object to a state block that doesn't have one yet
  void D3D8StateBlock::SetD3D9(Com<d3d9::IDirect3DStateBlock9>&& pStateBlock) {
    if (likely(m_stateBlock == nullptr)) {
      m_stateBlock = std::move(pStateBlock);
    } else {
      Logger::err("D3D8StateBlock::SetD3D9: m_stateBlock has already been initialized");
    }
  }

  HRESULT D3D8StateBlock::Capture() {
    if (unlikely(m_stateBlock == nullptr))
      return D3DERR_INVALIDCALL;

    if (m_captures.flags.test(D3D8CapturedStateFlag::Indices)) {
      m_state.baseVertexIndex = m_device->m_baseVertexIndex;
      m_state.indices = m_device->m_indices.ptr();
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::SWVP)) {
      DWORD swvpState;
      m_device->GetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, &swvpState);
      m_state.isSWVP = static_cast<bool>(swvpState);
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::VertexBuffers)) {
      for (DWORD stream = 0; stream < m_state.streams.size(); stream++) {
        if (m_captures.streams.get(stream)) {
          m_state.streams[stream].buffer = m_device->m_streams[stream].buffer.ptr();
          m_state.streams[stream].stride = m_device->m_streams[stream].stride;
        }
      }
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::Textures)) {
      for (DWORD stage = 0; stage < m_state.textures.size(); stage++) {
        if (m_captures.textures.get(stage))
          m_state.textures[stage] = m_device->m_textures[stage].ptr();
      }
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::VertexShader))
      m_device->GetVertexShader(&m_state.vertexShaderHandle);

    if (m_captures.flags.test(D3D8CapturedStateFlag::PixelShader))
      m_device->GetPixelShader(&m_state.pixelShaderHandle);

    return m_stateBlock->Capture();
  }

  HRESULT D3D8StateBlock::Apply() {
    if (unlikely(m_stateBlock == nullptr))
      return D3DERR_INVALIDCALL;

    HRESULT res = m_stateBlock->Apply();

    if (m_captures.flags.test(D3D8CapturedStateFlag::Indices))
      m_device->SetIndices(m_state.indices, m_state.baseVertexIndex);

    // This was a very easy footgun for D3D8 applications.
    if (m_captures.flags.test(D3D8CapturedStateFlag::SWVP))
      m_device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, static_cast<DWORD>(m_state.isSWVP));

    if (m_captures.flags.test(D3D8CapturedStateFlag::VertexBuffers)) {
      for (DWORD stream = 0; stream < m_state.streams.size(); stream++) {
        if (m_captures.streams.get(stream))
          m_device->SetStreamSource(stream, m_state.streams[stream].buffer, m_state.streams[stream].stride);
      }
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::Textures)) {
      for (DWORD stage = 0; stage < m_state.textures.size(); stage++) {
        if (m_captures.textures.get(stage))
          m_device->SetTexture(stage, m_state.textures[stage]);
      }
    }

    if (m_captures.flags.test(D3D8CapturedStateFlag::VertexShader))
      m_device->SetVertexShader(m_state.vertexShaderHandle);

    if (m_captures.flags.test(D3D8CapturedStateFlag::PixelShader))
      m_device->SetPixelShader(m_state.pixelShaderHandle);

    return res;
  }

}
