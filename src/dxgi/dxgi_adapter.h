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
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(
            REFGUID       InterfaceName,
            LARGE_INTEGER *pUMDVersion) final;
    
    HRESULT STDMETHODCALLTYPE EnumOutputs(
            UINT        Output,
            IDXGIOutput **ppOutput) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_ADAPTER_DESC *pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc1(
            DXGI_ADAPTER_DESC1 *pDesc) final;
    
    Rc<DxvkAdapter> STDMETHODCALLTYPE GetDXVKAdapter() final;
    
    DxgiFormatPair STDMETHODCALLTYPE LookupFormat(
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
