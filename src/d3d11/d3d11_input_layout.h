#pragma once

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11InputLayout : public D3D11DeviceChild<ID3D11InputLayout> {
    
  public:
    
    D3D11InputLayout(
            D3D11Device*                pDevice,
      const Rc<DxvkInputLayout>&        inputLayout);
    
    ~D3D11InputLayout();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(
            ID3D11Device **ppDevice) final;
    
    Rc<DxvkInputLayout> GetDXVKInputLayout() const {
      return m_inputLayout;
    }
    
  private:
    
    D3D11Device* const  m_device;
    Rc<DxvkInputLayout> m_inputLayout;
    
  };
  
}
