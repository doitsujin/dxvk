#pragma once

#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

#include <dxvk_adapter.h>

#include "dxgi_interfaces.h"
#include "dxgi_object.h"

namespace dxvk {
  
  class DxgiFactory;
  class DxgiOutput;
  
  class DxgiAdapter : public DxgiObject<IDXGIVkAdapter> {
    
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
    
    HRESULT STDMETHODCALLTYPE CreateDevice(
            IDXGIObject*              pContainer,
      const VkPhysicalDeviceFeatures* pFeatures,
            IDXGIVkDevice**           ppDevice) final;
    
    DxgiFormatInfo STDMETHODCALLTYPE LookupFormat(
            DXGI_FORMAT format, DxgiFormatMode mode) final;
    
    HRESULT GetOutputFromMonitor(
            HMONITOR              Monitor,
            IDXGIOutput**         ppOutput);
    
  private:
    
    using FormatMap = std::unordered_map<DXGI_FORMAT, DxgiFormatInfo>;
    
    Com<DxgiFactory>  m_factory;
    Com<DxgiOutput>   m_output;
    Rc<DxvkAdapter>   m_adapter;
    
    FormatMap         m_colorFormats;
    FormatMap         m_depthFormats;
    
    void AddColorFormatTypeless(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat);
    
    void AddColorFormat(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat,
            VkComponentMapping                swizzle = {
              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY });
    
    void AddDepthFormatTypeless(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat);
    
    void AddDepthFormat(
            DXGI_FORMAT                       srcFormat,
            VkFormat                          dstFormat,
            VkImageAspectFlags                srvAspect);
    
    void SetupOutputs();
    
    void SetupFormatTable();
    
  };

}
