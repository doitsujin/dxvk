#include "d3d8_device.h"
#include "d3d8_state_block.h"

namespace dxvk {

  D3D8StateBlock::D3D8StateBlock(
          D3D8Device*                       pDevice,
          D3DSTATEBLOCKTYPE                 Type,
          Com<d3d9::IDirect3DStateBlock9>&& pStateBlock)
    : m_device(pDevice)
    , m_stateBlock(std::move(pStateBlock))
    , m_type(Type)
    , m_isSWVP(pDevice->GetD3D9()->GetSoftwareVertexProcessing()) {
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

  // Construct a state block without a D3D9 object
  D3D8StateBlock::D3D8StateBlock(D3D8Device* pDevice)
    : D3D8StateBlock(pDevice, D3DSTATEBLOCKTYPE(0), nullptr) {
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

    if (m_capture.vs) m_device->GetVertexShader(&m_vertexShader);
    if (m_capture.ps) m_device->GetPixelShader(&m_pixelShader);

    for (DWORD stage = 0; stage < m_textures.size(); stage++) {
      if (m_capture.textures.get(stage))
        m_textures[stage] = m_device->m_textures[stage].ptr();
    }

    for (DWORD stream = 0; stream < m_streams.size(); stream++) {
      if (m_capture.streams.get(stream)) {
        m_streams[stream].buffer = m_device->m_streams[stream].buffer.ptr();
        m_streams[stream].stride = m_device->m_streams[stream].stride;
      }
    }

    if (m_capture.indices) {
      m_baseVertexIndex = m_device->m_baseVertexIndex;
      m_indices = m_device->m_indices.ptr();
    }

    if (m_capture.swvp) {
      DWORD swvpState;
      m_device->GetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, &swvpState);
      m_isSWVP = static_cast<bool>(swvpState);
    }

    return m_stateBlock->Capture();
  }

  HRESULT D3D8StateBlock::Apply() {
    if (unlikely(m_stateBlock == nullptr))
      return D3DERR_INVALIDCALL;

    HRESULT res = m_stateBlock->Apply();

    if (m_capture.vs) m_device->SetVertexShader(m_vertexShader);
    if (m_capture.ps) m_device->SetPixelShader(m_pixelShader);

    for (DWORD stage = 0; stage < m_textures.size(); stage++) {
      if (m_capture.textures.get(stage))
        m_device->SetTexture(stage, m_textures[stage]);
    }

    for (DWORD stream = 0; stream < m_streams.size(); stream++) {
      if (m_capture.streams.get(stream))
        m_device->SetStreamSource(stream, m_streams[stream].buffer, m_streams[stream].stride);
    }

    if (m_capture.indices)
      m_device->SetIndices(m_indices, m_baseVertexIndex);

    // This was a very easy footgun for D3D8 applications.
    if (m_capture.swvp)
      m_device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, static_cast<DWORD>(m_isSWVP));

    return res;
  }

}
