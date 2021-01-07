#pragma once

#include "d3d11_device.h"
#include "d3d11_context_state.h"
#include "d3d11_device_child.h"

namespace dxvk {

  /**
   * \brief Device context state implementation
   *
   * This is an opaque interface in D3D11, and we only
   * implement the state block-like functionality, not
   * the methods to disable certain context and device
   * interfaces based on the emulated device IID.
   */
  class D3D11DeviceContextState : public D3D11DeviceChild<ID3DDeviceContextState> {

  public:

    D3D11DeviceContextState(
            D3D11Device*         pDevice);
    
    ~D3D11DeviceContextState();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);
    
    void SetState(const D3D11ContextState& State) {
      m_state = State;
    }

    void GetState(D3D11ContextState& State) const {
      State = m_state;
    }

  private:

    D3D11ContextState m_state;

  };

}
