#include "d3d8_device.h"
#include "d3d8_state_block.h"

HRESULT dxvk::D3D8StateBlock::Capture() {

  if (unlikely(m_stateBlock == nullptr)) return D3DERR_INVALIDCALL;

  if (m_capture.vs) m_device->GetVertexShader(&m_vertexShader);
  if (m_capture.ps) m_device->GetPixelShader(&m_pixelShader);

  if (m_capture.indices) m_device->GetIndices(&m_indices, &m_baseVertexIndex);

  return m_stateBlock->Capture();
}

HRESULT dxvk::D3D8StateBlock::Apply() {

  if (unlikely(m_stateBlock == nullptr)) return D3DERR_INVALIDCALL;


  HRESULT res = m_stateBlock->Apply();

  if (m_capture.vs) m_device->SetVertexShader(m_vertexShader);
  if (m_capture.ps) m_device->SetPixelShader(m_pixelShader);

  if (m_capture.indices) m_device->SetIndices(m_indices, m_baseVertexIndex);

  return res;
}
