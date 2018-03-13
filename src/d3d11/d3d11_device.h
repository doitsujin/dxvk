#pragma once

#include <mutex>
#include <vector>

#include "../dxbc/dxbc_options.h"

#include "../dxgi/dxgi_object.h"

#include "../util/com/com_private_data.h"

#include "d3d11_interfaces.h"
#include "d3d11_state.h"
#include "d3d11_util.h"

namespace dxvk {
  class DxgiAdapter;
  
  class D3D11Buffer;
  class D3D11Counter;
  class D3D11DeviceContext;
  class D3D11ImmediateContext;
  class D3D11Predicate;
  class D3D11PresentDevice;
  class D3D11Query;
  class D3D11ShaderModule;
  class D3D11Texture1D;
  class D3D11Texture2D;
  class D3D11Texture3D;
  
  class D3D11Device : public ComObject<ID3D11Device> {
    /// Maximum number of resource init commands per command buffer
    constexpr static uint64_t InitCommandThreshold = 50;
  public:
    
    D3D11Device(
            IDXGIDevicePrivate*     dxgiDevice,
            D3D_FEATURE_LEVEL       featureLevel,
            UINT                    featureFlags);
    ~D3D11Device();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE CreateBuffer(
      const D3D11_BUFFER_DESC*      pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Buffer**          ppBuffer) final;
    
    HRESULT STDMETHODCALLTYPE CreateTexture1D(
      const D3D11_TEXTURE1D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture1D**       ppTexture1D) final;
    
    HRESULT STDMETHODCALLTYPE CreateTexture2D(
      const D3D11_TEXTURE2D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture2D**       ppTexture2D) final;
    
    HRESULT STDMETHODCALLTYPE CreateTexture3D(
      const D3D11_TEXTURE3D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture3D**       ppTexture3D) final;
    
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
            ID3D11Resource*                   pResource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
            ID3D11ShaderResourceView**        ppSRView) final;
    
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
            ID3D11Resource*                   pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
            ID3D11UnorderedAccessView**       ppUAView) final;
    
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
            ID3D11Resource*                   pResource,
      const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
            ID3D11RenderTargetView**          ppRTView) final;
    
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
            ID3D11Resource*                   pResource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
            ID3D11DepthStencilView**          ppDepthStencilView) final;
    
    HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
            UINT                        NumElements,
      const void*                       pShaderBytecodeWithInputSignature,
            SIZE_T                      BytecodeLength,
            ID3D11InputLayout**         ppInputLayout) final;
    
    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11VertexShader**        ppVertexShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
            UINT                        NumEntries,
      const UINT*                       pBufferStrides,
            UINT                        NumStrides,
            UINT                        RasterizedStream,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader) final;
    
    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11PixelShader**         ppPixelShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateHullShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11HullShader**          ppHullShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateDomainShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11DomainShader**        ppDomainShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateComputeShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11ComputeShader**       ppComputeShader) final;
    
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(
            ID3D11ClassLinkage**        ppLinkage) final;
    
    HRESULT STDMETHODCALLTYPE CreateBlendState(
      const D3D11_BLEND_DESC*           pBlendStateDesc,
            ID3D11BlendState**          ppBlendState) final;
    
    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
            ID3D11DepthStencilState**   ppDepthStencilState) final;
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(
      const D3D11_RASTERIZER_DESC*      pRasterizerDesc,
            ID3D11RasterizerState**     ppRasterizerState) final;
    
    HRESULT STDMETHODCALLTYPE CreateSamplerState(
      const D3D11_SAMPLER_DESC*         pSamplerDesc,
            ID3D11SamplerState**        ppSamplerState) final;
    
    HRESULT STDMETHODCALLTYPE CreateQuery(
      const D3D11_QUERY_DESC*           pQueryDesc,
            ID3D11Query**               ppQuery) final;
    
    HRESULT STDMETHODCALLTYPE CreatePredicate(
      const D3D11_QUERY_DESC*           pPredicateDesc,
            ID3D11Predicate**           ppPredicate) final;
    
    HRESULT STDMETHODCALLTYPE CreateCounter(
      const D3D11_COUNTER_DESC*         pCounterDesc,
            ID3D11Counter**             ppCounter) final;
    
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(
            UINT                        ContextFlags,
            ID3D11DeviceContext**       ppDeferredContext) final;
    
    HRESULT STDMETHODCALLTYPE OpenSharedResource(
            HANDLE      hResource,
            REFIID      ReturnedInterface,
            void**      ppResource) final;
    
    HRESULT STDMETHODCALLTYPE CheckFormatSupport(
            DXGI_FORMAT Format,
            UINT*       pFormatSupport) final;
    
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
            DXGI_FORMAT Format,
            UINT        SampleCount,
            UINT*       pNumQualityLevels) final;
    
    void STDMETHODCALLTYPE CheckCounterInfo(
            D3D11_COUNTER_INFO* pCounterInfo) final;
    
    HRESULT STDMETHODCALLTYPE CheckCounter(
      const D3D11_COUNTER_DESC* pDesc,
            D3D11_COUNTER_TYPE* pType,
            UINT*               pActiveCounters,
            LPSTR               szName,
            UINT*               pNameLength,
            LPSTR               szUnits,
            UINT*               pUnitsLength,
            LPSTR               szDescription,
            UINT*               pDescriptionLength) final;
    
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
            D3D11_FEATURE Feature,
            void*         pFeatureSupportData,
            UINT          FeatureSupportDataSize) final;
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID Name,
            UINT    *pDataSize,
            void    *pData) final;
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID Name,
            UINT    DataSize,
      const void    *pData) final;
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID  Name,
      const IUnknown *pUnknown) final;
    
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() final;
    
    UINT STDMETHODCALLTYPE GetCreationFlags() final;
    
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() final;
    
    void STDMETHODCALLTYPE GetImmediateContext(
            ID3D11DeviceContext** ppImmediateContext) final;
    
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) final;
    
    UINT STDMETHODCALLTYPE GetExceptionMode() final;
    
    Rc<DxvkDevice> GetDXVKDevice() {
      return m_dxvkDevice;
    }
    
    DxvkBufferSlice AllocateCounterSlice();
    
    void FreeCounterSlice(const DxvkBufferSlice& Slice);
    
    void FlushInitContext();
    
    VkPipelineStageFlags GetEnabledShaderStages() const;
    
    DxgiFormatInfo STDMETHODCALLTYPE LookupFormat(
            DXGI_FORMAT           format,
            DxgiFormatMode        mode) const;
    
    static bool CheckFeatureLevelSupport(
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
    static VkPhysicalDeviceFeatures GetDeviceFeatures(
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
  private:
    
    const Com<IDXGIDevicePrivate>   m_dxgiDevice;
          Com<IDXGIAdapterPrivate>  m_dxgiAdapter;
    const Com<D3D11PresentDevice>   m_presentDevice;
    
    const D3D_FEATURE_LEVEL         m_featureLevel;
    const UINT                      m_featureFlags;
    
    const Rc<DxvkDevice>            m_dxvkDevice;
    const Rc<DxvkAdapter>           m_dxvkAdapter;
    
    const DxbcOptions               m_dxbcOptions;
    
    D3D11ImmediateContext*          m_context = nullptr;
    
    std::mutex                      m_counterMutex;
    std::vector<uint32_t>           m_counterSlices;
    Rc<DxvkBuffer>                  m_counterBuffer;
    
    std::mutex                      m_resourceInitMutex;
    Rc<DxvkContext>                 m_resourceInitContext;
    uint64_t                        m_resourceInitCommands = 0;
    
    D3D11StateObjectSet<D3D11BlendState>        m_bsStateObjects;
    D3D11StateObjectSet<D3D11DepthStencilState> m_dsStateObjects;
    D3D11StateObjectSet<D3D11RasterizerState>   m_rsStateObjects;
    D3D11StateObjectSet<D3D11SamplerState>      m_samplerObjects;
    
    HRESULT CreateShaderModule(
            D3D11ShaderModule*      pShaderModule,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
            ID3D11ClassLinkage*     pClassLinkage);
    
    void InitBuffer(
            D3D11Buffer*                pBuffer,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
    void InitTexture(
      const Rc<DxvkImage>&              image,
      const D3D11_SUBRESOURCE_DATA*     pInitialData);
    
    HRESULT GetShaderResourceViewDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc);
    
    HRESULT GetUnorderedAccessViewDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
    
    HRESULT GetRenderTargetViewDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC*    pDesc);
    
    HRESULT GetDepthStencilViewDescFromResource(
            ID3D11Resource*                   pResource,
            D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc);
    
    HRESULT SetShaderResourceViewDescUnspecValues(
            ID3D11Resource*                   pResource,
            D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc);
    
    HRESULT SetUnorderedAccessViewDescUnspecValues(
            ID3D11Resource*                   pResource,
            D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
    
    HRESULT SetRenderTargetViewDescUnspecValues(
            ID3D11Resource*                   pResource,
            D3D11_RENDER_TARGET_VIEW_DESC*    pDesc);
    
    HRESULT SetDepthStencilViewDescUnspecValues(
            ID3D11Resource*                   pResource,
            D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc);
    
    HRESULT GetFormatSupportFlags(DXGI_FORMAT Format, UINT* pFlags) const;
    
    void CreateCounterBuffer();
    
    void LockResourceInitContext();
    void UnlockResourceInitContext(uint64_t CommandCount);
    void SubmitResourceInitCommands();
    
    static D3D_FEATURE_LEVEL GetMaxFeatureLevel();
    
  };
  
}
