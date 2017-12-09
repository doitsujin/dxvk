#pragma once

#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <vector>

#include <dxvk_adapter.h>

#include "dxgi_interfaces.h"
#include "dxgi_object.h"

namespace dxvk {
  
  class DxgiFactory;
  class DxgiOutput;
  
  class DxgiAdapter : public DxgiObject<IDXGIAdapterPrivate> {
    
  public:
    
    DxgiAdapter(
            DxgiFactory*      factory,
      const Rc<DxvkAdapter>&  adapter);
    ~DxgiAdapter();
    
    HRESULT QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    HRESULT CheckInterfaceSupport(
            REFGUID       InterfaceName,
            LARGE_INTEGER *pUMDVersion) final;
    
    HRESULT EnumOutputs(
            UINT        Output,
            IDXGIOutput **ppOutput) final;
    
    HRESULT GetDesc(
            DXGI_ADAPTER_DESC *pDesc) final;
    
    HRESULT GetDesc1(
            DXGI_ADAPTER_DESC1 *pDesc) final;
    
    Rc<DxvkAdapter> GetDXVKAdapter() final;
    
    DxgiFormatPair LookupFormat(
            DXGI_FORMAT format) final;
    
  private:
    
    Com<DxgiFactory>  m_factory;
    Rc<DxvkAdapter>   m_adapter;
    
    std::unordered_map<DXGI_FORMAT, DxgiFormatPair> m_formats;
    
    void AddFormat(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat);
    
    void AddFormat(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat,
      const std::initializer_list<VkFormat>&  fallbacks,
            VkFormatFeatureFlags              features);
    
    void SetupFormatTable();
    
    bool HasFormatSupport(
            VkFormat                          format,
            VkFormatFeatureFlags              features) const;
    
  };

}
