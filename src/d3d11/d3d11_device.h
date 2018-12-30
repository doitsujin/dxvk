#pragma once

#include <mutex>
#include <vector>

#include "../dxbc/dxbc_options.h"

#include "../dxgi/dxgi_object.h"
#include "../dxgi/dxgi_interfaces.h"

#include "../dxvk/dxvk_cs.h"

#include "../d3d10/d3d10_device.h"

#include "../util/com/com_private_data.h"

#include "d3d11_counter_buffer.h"
#include "d3d11_initializer.h"
#include "d3d11_interfaces.h"
#include "d3d11_interop.h"
#include "d3d11_options.h"
#include "d3d11_present.h"
#include "d3d11_shader.h"
#include "d3d11_state.h"
#include "d3d11_util.h"

namespace dxvk {
  class DxgiAdapter;
  
  class D3D11Buffer;
  class D3D11CommonShader;
  class D3D11CommonTexture;
  class D3D11Counter;
  class D3D11DeviceContext;
  class D3D11DXGIDevice;
  class D3D11ImmediateContext;
  class D3D11Predicate;
  class D3D11Query;
  class D3D11Texture1D;
  class D3D11Texture2D;
  class D3D11Texture3D;
  
  /**
   * \brief D3D11 device implementation
   * 
   * Implements the ID3D11Device interfaces
   * as part of a \ref D3D11DeviceContainer.
   */
  class D3D11Device final : public ID3D11Device1 {
    /// Maximum number of resource init commands per command buffer
    constexpr static uint64_t InitCommandThreshold = 50;
  public:
    
    D3D11Device(
            D3D11DXGIDevice*        pContainer,
            D3D_FEATURE_LEVEL       FeatureLevel,
            UINT                    FeatureFlags);
    
    ~D3D11Device();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateBuffer(
      const D3D11_BUFFER_DESC*      pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Buffer**          ppBuffer);
    
    HRESULT STDMETHODCALLTYPE CreateTexture1D(
      const D3D11_TEXTURE1D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture1D**       ppTexture1D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture2D(
      const D3D11_TEXTURE2D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture2D**       ppTexture2D);
    
    HRESULT STDMETHODCALLTYPE CreateTexture3D(
      const D3D11_TEXTURE3D_DESC*   pDesc,
      const D3D11_SUBRESOURCE_DATA* pInitialData,
            ID3D11Texture3D**       ppTexture3D);
    
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
            ID3D11Resource*                   pResource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
            ID3D11ShaderResourceView**        ppSRView);
    
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
            ID3D11Resource*                   pResource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
            ID3D11UnorderedAccessView**       ppUAView);
    
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
            ID3D11Resource*                   pResource,
      const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
            ID3D11RenderTargetView**          ppRTView);
    
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
            ID3D11Resource*                   pResource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
            ID3D11DepthStencilView**          ppDepthStencilView);
    
    HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
            UINT                        NumElements,
      const void*                       pShaderBytecodeWithInputSignature,
            SIZE_T                      BytecodeLength,
            ID3D11InputLayout**         ppInputLayout);
    
    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11VertexShader**        ppVertexShader);
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader);
    
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
            UINT                        NumEntries,
      const UINT*                       pBufferStrides,
            UINT                        NumStrides,
            UINT                        RasterizedStream,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11GeometryShader**      ppGeometryShader);
    
    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11PixelShader**         ppPixelShader);
    
    HRESULT STDMETHODCALLTYPE CreateHullShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11HullShader**          ppHullShader);
    
    HRESULT STDMETHODCALLTYPE CreateDomainShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11DomainShader**        ppDomainShader);
    
    HRESULT STDMETHODCALLTYPE CreateComputeShader(
      const void*                       pShaderBytecode,
            SIZE_T                      BytecodeLength,
            ID3D11ClassLinkage*         pClassLinkage,
            ID3D11ComputeShader**       ppComputeShader);
    
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(
            ID3D11ClassLinkage**        ppLinkage);
    
    HRESULT STDMETHODCALLTYPE CreateBlendState(
      const D3D11_BLEND_DESC*           pBlendStateDesc,
            ID3D11BlendState**          ppBlendState);
    
    HRESULT STDMETHODCALLTYPE CreateBlendState1(
      const D3D11_BLEND_DESC1*          pBlendStateDesc,
            ID3D11BlendState1**         ppBlendState);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
            ID3D11DepthStencilState**   ppDepthStencilState);
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(
      const D3D11_RASTERIZER_DESC*      pRasterizerDesc,
            ID3D11RasterizerState**     ppRasterizerState);
    
    HRESULT STDMETHODCALLTYPE CreateRasterizerState1(
      const D3D11_RASTERIZER_DESC1*     pRasterizerDesc,
            ID3D11RasterizerState1**    ppRasterizerState);
    
    HRESULT STDMETHODCALLTYPE CreateSamplerState(
      const D3D11_SAMPLER_DESC*         pSamplerDesc,
            ID3D11SamplerState**        ppSamplerState);
    
    HRESULT STDMETHODCALLTYPE CreateQuery(
      const D3D11_QUERY_DESC*           pQueryDesc,
            ID3D11Query**               ppQuery);
    
    HRESULT STDMETHODCALLTYPE CreatePredicate(
      const D3D11_QUERY_DESC*           pPredicateDesc,
            ID3D11Predicate**           ppPredicate);
    
    HRESULT STDMETHODCALLTYPE CreateCounter(
      const D3D11_COUNTER_DESC*         pCounterDesc,
            ID3D11Counter**             ppCounter);
    
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(
            UINT                        ContextFlags,
            ID3D11DeviceContext**       ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
            UINT                        ContextFlags,
            ID3D11DeviceContext1**      ppDeferredContext);

    HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
            UINT                        Flags,
      const D3D_FEATURE_LEVEL*          pFeatureLevels,
            UINT                        FeatureLevels,
            UINT                        SDKVersion,
            REFIID                      EmulatedInterface,
            D3D_FEATURE_LEVEL*          pChosenFeatureLevel,
            ID3DDeviceContextState**    ppContextState);
    
    HRESULT STDMETHODCALLTYPE OpenSharedResource(
            HANDLE      hResource,
            REFIID      ReturnedInterface,
            void**      ppResource);

    HRESULT STDMETHODCALLTYPE OpenSharedResource1(
            HANDLE      hResource,
            REFIID      returnedInterface,
            void**      ppResource);

    HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(
            LPCWSTR     lpName,
            DWORD       dwDesiredAccess,
            REFIID      returnedInterface,
            void**      ppResource);
    
    HRESULT STDMETHODCALLTYPE CheckFormatSupport(
            DXGI_FORMAT Format,
            UINT*       pFormatSupport);
    
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
            DXGI_FORMAT Format,
            UINT        SampleCount,
            UINT*       pNumQualityLevels);
    
    void STDMETHODCALLTYPE CheckCounterInfo(
            D3D11_COUNTER_INFO* pCounterInfo);
    
    HRESULT STDMETHODCALLTYPE CheckCounter(
      const D3D11_COUNTER_DESC* pDesc,
            D3D11_COUNTER_TYPE* pType,
            UINT*               pActiveCounters,
            LPSTR               szName,
            UINT*               pNameLength,
            LPSTR               szUnits,
            UINT*               pUnitsLength,
            LPSTR               szDescription,
            UINT*               pDescriptionLength);
    
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
            D3D11_FEATURE Feature,
            void*         pFeatureSupportData,
            UINT          FeatureSupportDataSize);
    
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID Name,
            UINT    *pDataSize,
            void    *pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID Name,
            UINT    DataSize,
      const void    *pData);
    
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID  Name,
      const IUnknown *pUnknown);
    
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel();
    
    UINT STDMETHODCALLTYPE GetCreationFlags();
    
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason();
    
    void STDMETHODCALLTYPE GetImmediateContext(
            ID3D11DeviceContext** ppImmediateContext);

    void STDMETHODCALLTYPE GetImmediateContext1(
            ID3D11DeviceContext1** ppImmediateContext);
    
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags);
    
    UINT STDMETHODCALLTYPE GetExceptionMode();
    
    Rc<DxvkDevice> GetDXVKDevice() {
      return m_dxvkDevice;
    }
    
    void FlushInitContext();
    
    VkPipelineStageFlags GetEnabledShaderStages() const;
    
    DXGI_VK_FORMAT_INFO LookupFormat(
            DXGI_FORMAT           Format,
            DXGI_VK_FORMAT_MODE   Mode) const;
    
    DXGI_VK_FORMAT_FAMILY LookupFamily(
            DXGI_FORMAT           Format,
            DXGI_VK_FORMAT_MODE   Mode) const;
    
    DxvkCsChunkRef AllocCsChunk(DxvkCsChunkFlags flags) {
      DxvkCsChunk* chunk = m_csChunkPool.allocChunk(flags);
      return DxvkCsChunkRef(chunk, &m_csChunkPool);
    }
    
    const D3D11Options* GetOptions() const {
      return &m_d3d11Options;
    }

    D3D10Device* GetD3D10Interface() const {
      return m_d3d10Device;
    }
    
    DxvkBufferSlice AllocUavCounterSlice() { return m_uavCounters->AllocSlice(); }
    DxvkBufferSlice AllocXfbCounterSlice() { return m_xfbCounters->AllocSlice(); }
    
    void FreeUavCounterSlice(const DxvkBufferSlice& Slice) { m_uavCounters->FreeSlice(Slice); }
    void FreeXfbCounterSlice(const DxvkBufferSlice& Slice) { m_xfbCounters->FreeSlice(Slice); }
    
    static bool CheckFeatureLevelSupport(
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
    static DxvkDeviceFeatures GetDeviceFeatures(
      const Rc<DxvkAdapter>&  adapter,
            D3D_FEATURE_LEVEL featureLevel);
    
  private:
    
    IDXGIObject*                    m_container;

    const D3D_FEATURE_LEVEL         m_featureLevel;
    const UINT                      m_featureFlags;
    
    const Rc<DxvkDevice>            m_dxvkDevice;
    const Rc<DxvkAdapter>           m_dxvkAdapter;
    
    const DXGIVkFormatTable         m_d3d11Formats;
    const D3D11Options              m_d3d11Options;
    const DxbcOptions               m_dxbcOptions;
    
    DxvkCsChunkPool                 m_csChunkPool;
    
    D3D11Initializer*               m_initializer = nullptr;
    D3D11ImmediateContext*          m_context     = nullptr;
    D3D10Device*                    m_d3d10Device = nullptr;

    Rc<D3D11CounterBuffer>          m_uavCounters;
    Rc<D3D11CounterBuffer>          m_xfbCounters;
    
    D3D11StateObjectSet<D3D11BlendState>        m_bsStateObjects;
    D3D11StateObjectSet<D3D11DepthStencilState> m_dsStateObjects;
    D3D11StateObjectSet<D3D11RasterizerState>   m_rsStateObjects;
    D3D11StateObjectSet<D3D11SamplerState>      m_samplerObjects;
    D3D11ShaderModuleSet                        m_shaderModules;
    
    Rc<D3D11CounterBuffer> CreateUAVCounterBuffer();
    Rc<D3D11CounterBuffer> CreateXFBCounterBuffer();

    HRESULT CreateShaderModule(
            D3D11CommonShader*      pShaderModule,
            DxvkShaderKey           ShaderKey,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
            ID3D11ClassLinkage*     pClassLinkage,
      const DxbcModuleInfo*         pModuleInfo);
    
    HRESULT GetFormatSupportFlags(
            DXGI_FORMAT Format,
            UINT*       pFlags1,
            UINT*       pFlags2) const;
    
    BOOL GetImageTypeSupport(
            VkFormat    Format,
            VkImageType Type) const;
    
    static D3D_FEATURE_LEVEL GetMaxFeatureLevel(
      const Rc<DxvkAdapter>&        Adapter);
    
  };
  
  
  /**
   * \brief DXGI swap chain factory
   */
  class WineDXGISwapChainFactory : public IWineDXGISwapChainFactory {
    
  public:
    
    WineDXGISwapChainFactory(
            IDXGIVkPresentDevice*   pDevice);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
            IDXGIFactory*           pFactory,
            HWND                    hWnd,
      const DXGI_SWAP_CHAIN_DESC1*  pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
            IDXGIOutput*            pRestrictToOutput,
            IDXGISwapChain1**       ppSwapChain);
    
  private:
    
    IDXGIVkPresentDevice* m_device;
    
  };
  

  /**
   * \brief D3D11 device container
   * 
   * Stores all the objects that contribute to the D3D11
   * device implementation, including the DXGI device.
   */
  class D3D11DXGIDevice : public DxgiObject<IDXGIDevice3> {
    constexpr static uint32_t DefaultFrameLatency = 3;
  public:
    
    D3D11DXGIDevice(
          IDXGIAdapter*       pAdapter,
          DxvkAdapter*        pDxvkAdapter,
          D3D_FEATURE_LEVEL   FeatureLevel,
          UINT                FeatureFlags);
    
    ~D3D11DXGIDevice();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                riid,
            void**                ppParent);
    
    HRESULT STDMETHODCALLTYPE CreateSurface(
      const DXGI_SURFACE_DESC*    pDesc,
            UINT                  NumSurfaces,
            DXGI_USAGE            Usage,
      const DXGI_SHARED_RESOURCE* pSharedResource,
            IDXGISurface**        ppSurface) final;
    
    HRESULT STDMETHODCALLTYPE GetAdapter(
            IDXGIAdapter**        pAdapter) final;
    
    HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(
            INT*                  pPriority) final;
    
    HRESULT STDMETHODCALLTYPE QueryResourceResidency(
            IUnknown* const*      ppResources,
            DXGI_RESIDENCY*       pResidencyStatus,
            UINT                  NumResources) final;
    
    HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(
            INT                   Priority) final;
    
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                 pMaxLatency) final;
    
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                  MaxLatency) final;

    HRESULT STDMETHODCALLTYPE OfferResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            DXGI_OFFER_RESOURCE_PRIORITY  Priority) final;
        
    HRESULT STDMETHODCALLTYPE ReclaimResources( 
            UINT                          NumResources,
            IDXGIResource* const*         ppResources,
            BOOL*                         pDiscarded) final;
        
    HRESULT STDMETHODCALLTYPE EnqueueSetEvent( 
            HANDLE                hEvent) final;
    
    void STDMETHODCALLTYPE Trim() final;
    
    Rc<DxvkEvent> STDMETHODCALLTYPE GetFrameSyncEvent();

    Rc<DxvkDevice> STDMETHODCALLTYPE GetDXVKDevice();

  private:

    Com<IDXGIAdapter>   m_dxgiAdapter;

    Rc<DxvkAdapter>     m_dxvkAdapter;
    Rc<DxvkDevice>      m_dxvkDevice;

    D3D11Device         m_d3d11Device;
    D3D11PresentDevice  m_d3d11Presenter;
    D3D11VkInterop      m_d3d11Interop;
    
    WineDXGISwapChainFactory m_wineFactory;
    
    uint32_t m_frameLatencyCap = 0;
    uint32_t m_frameLatency    = DefaultFrameLatency;
    uint32_t m_frameId         = 0;

    std::array<Rc<DxvkEvent>, 16> m_frameEvents;

    Rc<DxvkDevice> CreateDevice(D3D_FEATURE_LEVEL FeatureLevel);

  };
  
}
