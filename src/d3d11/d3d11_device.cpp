#include <algorithm>
#include <cstring>

#include "../dxgi/dxgi_monitor.h"
#include "../dxgi/dxgi_surface.h"
#include "../dxgi/dxgi_swapchain.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_instance.h"

#include "d3d11_buffer.h"
#include "d3d11_class_linkage.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_fence.h"
#include "d3d11_input_layout.h"
#include "d3d11_interop.h"
#include "d3d11_query.h"
#include "d3d11_resource.h"
#include "d3d11_sampler.h"
#include "d3d11_shader.h"
#include "d3d11_state_object.h"
#include "d3d11_swapchain.h"
#include "d3d11_texture.h"
#include "d3d11_video.h"

#include "../wsi/wsi_window.h"

#include "../util/util_shared_res.h"

namespace dxvk {
  
  constexpr uint32_t D3D11DXGIDevice::DefaultFrameLatency;



  D3D11Device::D3D11Device(
          D3D11DXGIDevice*    pContainer,
          D3D_FEATURE_LEVEL   FeatureLevel,
          UINT                FeatureFlags)
  : m_container         (pContainer),
    m_featureLevel      (FeatureLevel),
    m_featureFlags      (FeatureFlags),
    m_dxvkDevice        (pContainer->GetDXVKDevice()),
    m_dxvkAdapter       (m_dxvkDevice->adapter()),
    m_d3d11Formats      (m_dxvkDevice),
    m_d3d11Options      (m_dxvkDevice->instance()->config()),
    m_dxbcOptions       (m_dxvkDevice, m_d3d11Options),
    m_maxFeatureLevel   (GetMaxFeatureLevel(m_dxvkDevice->instance(), m_dxvkDevice->adapter())),
    m_deviceFeatures    (m_dxvkDevice->instance(), m_dxvkDevice->adapter(), m_d3d11Options, m_featureLevel) {
    m_initializer = new D3D11Initializer(this);
    m_context     = new D3D11ImmediateContext(this, m_dxvkDevice);
    m_d3d10Device = new D3D10Device(this, m_context.ptr());
  }
  
  
  D3D11Device::~D3D11Device() {
    delete m_d3d10Device;
    m_context = nullptr;
    delete m_initializer;
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11Device::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11Device::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid, void** ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
    
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC*      pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Buffer**          ppBuffer) {
    InitReturnPtr(ppBuffer);
    
    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_BUFFER_DESC desc = *pDesc;
    HRESULT hr = D3D11Buffer::NormalizeBufferProperties(&desc);

    if (FAILED(hr))
      return hr;

    if ((desc.MiscFlags & (D3D11_RESOURCE_MISC_TILED | D3D11_RESOURCE_MISC_TILE_POOL))
     && !m_deviceFeatures.GetTiledResourcesTier())
      return E_INVALIDARG;

    if (!ppBuffer)
      return S_FALSE;
    
    try {
      const Com<D3D11Buffer> buffer = new D3D11Buffer(this, &desc, nullptr);

      if (!(desc.MiscFlags & D3D11_RESOURCE_MISC_TILE_POOL))
        m_initializer->InitBuffer(buffer.ptr(), pInitialData);

      *ppBuffer = buffer.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture1D**       ppTexture1D) {
    InitReturnPtr(ppTexture1D);

    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = 1;
    desc.Depth          = 1;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = DXGI_SAMPLE_DESC { 1, 0 };
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    
    HRESULT hr = D3D11CommonTexture::NormalizeTextureProperties(&desc);

    if (FAILED(hr))
      return hr;

    if (desc.MiscFlags & D3D11_RESOURCE_MISC_TILED)
      return E_INVALIDARG;

    if (!ppTexture1D)
      return S_FALSE;
    
    try {
      const Com<D3D11Texture1D> texture = new D3D11Texture1D(this, &desc, nullptr);
      m_initializer->InitTexture(texture->GetCommonTexture(), pInitialData);
      *ppTexture1D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture2D**       ppTexture2D) {
    InitReturnPtr(ppTexture2D);

    if (!pDesc)
      return E_INVALIDARG;

    D3D11_TEXTURE2D_DESC1 desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = pDesc->SampleDesc;
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    
    ID3D11Texture2D1* texture2D = nullptr;
    HRESULT hr = CreateTexture2D1(&desc, pInitialData, ppTexture2D ? &texture2D : nullptr);

    if (hr != S_OK)
      return hr;
    
    *ppTexture2D = texture2D;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D1(
    const D3D11_TEXTURE2D_DESC1*  pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture2D1**      ppTexture2D) {
    InitReturnPtr(ppTexture2D);

    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = 1;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = pDesc->SampleDesc;
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = pDesc->TextureLayout;
    
    HRESULT hr = D3D11CommonTexture::NormalizeTextureProperties(&desc);

    if ((desc.MiscFlags & D3D11_RESOURCE_MISC_TILED)
     && !m_deviceFeatures.GetTiledResourcesTier())
      return E_INVALIDARG;

    if (FAILED(hr))
      return hr;
    
    if (!ppTexture2D)
      return S_FALSE;
    
    try {
      Com<D3D11Texture2D> texture = new D3D11Texture2D(this, &desc, nullptr, nullptr);
      m_initializer->InitTexture(texture->GetCommonTexture(), pInitialData);
      *ppTexture2D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }

  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture3D**       ppTexture3D) {
    InitReturnPtr(ppTexture3D);

    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_TEXTURE3D_DESC1 desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = pDesc->Depth;
    desc.MipLevels      = pDesc->MipLevels;
    desc.Format         = pDesc->Format;
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    
    ID3D11Texture3D1* texture3D = nullptr;
    HRESULT hr = CreateTexture3D1(&desc, pInitialData, ppTexture3D ? &texture3D : nullptr);

    if (hr != S_OK)
      return hr;
    
    *ppTexture3D = texture3D;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D1(
    const D3D11_TEXTURE3D_DESC1*  pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture3D1**      ppTexture3D) {
    InitReturnPtr(ppTexture3D);

    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = pDesc->Depth;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = 1;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = DXGI_SAMPLE_DESC { 1, 0 };
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = pDesc->TextureLayout;
    
    HRESULT hr = D3D11CommonTexture::NormalizeTextureProperties(&desc);

    if (FAILED(hr))
      return hr;

    if ((desc.MiscFlags & D3D11_RESOURCE_MISC_TILED)
     && (m_deviceFeatures.GetTiledResourcesTier() < D3D11_TILED_RESOURCES_TIER_3))
      return E_INVALIDARG;

    if (!ppTexture3D)
      return S_FALSE;
      
    try {
      Com<D3D11Texture3D> texture = new D3D11Texture3D(this, &desc, nullptr);
      m_initializer->InitTexture(texture->GetCommonTexture(), pInitialData);
      *ppTexture3D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(
          ID3D11Resource*                   pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
          ID3D11ShaderResourceView**        ppSRView) {
    InitReturnPtr(ppSRView);

    uint32_t plane = GetViewPlaneIndex(pResource, pDesc ? pDesc->Format : DXGI_FORMAT_UNKNOWN);

    D3D11_SHADER_RESOURCE_VIEW_DESC1 desc = pDesc
      ? D3D11ShaderResourceView::PromoteDesc(pDesc, plane)
      : D3D11_SHADER_RESOURCE_VIEW_DESC1();
    
    ID3D11ShaderResourceView1* view = nullptr;

    HRESULT hr = CreateShaderResourceView1(pResource,
      pDesc    ? &desc : nullptr,
      ppSRView ? &view : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppSRView = view;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView1(
          ID3D11Resource*                   pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc,
          ID3D11ShaderResourceView1**       ppSRView) {
    InitReturnPtr(ppSRView);

    if (!pResource)
      return E_INVALIDARG;
    
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);
    
    // The description is optional. If omitted, we'll create
    // a view that covers all subresources of the image.
    D3D11_SHADER_RESOURCE_VIEW_DESC1 desc;
    
    if (!pDesc) {
      if (FAILED(D3D11ShaderResourceView::GetDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(D3D11ShaderResourceView::NormalizeDesc(pResource, &desc)))
        return E_INVALIDARG;
    }

    uint32_t plane = D3D11ShaderResourceView::GetPlaneSlice(&desc);
    
    if (!CheckResourceViewCompatibility(pResource, D3D11_BIND_SHADER_RESOURCE, desc.Format, plane)) {
      Logger::err(str::format("D3D11: Cannot create shader resource view:",
        "\n  Resource type:   ", resourceDesc.Dim,
        "\n  Resource usage:  ", resourceDesc.BindFlags,
        "\n  Resource format: ", resourceDesc.Format,
        "\n  View format:     ", desc.Format,
        "\n  View plane:      ", plane));
      return E_INVALIDARG;
    }
    
    if (!ppSRView)
      return S_FALSE;
    
    try {
      *ppSRView = ref(new D3D11ShaderResourceView(this, pResource, &desc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(
          ID3D11Resource*                   pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
          ID3D11UnorderedAccessView**       ppUAView) {
    InitReturnPtr(ppUAView);

    uint32_t plane = GetViewPlaneIndex(pResource, pDesc ? pDesc->Format : DXGI_FORMAT_UNKNOWN);

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 desc = pDesc
      ? D3D11UnorderedAccessView::PromoteDesc(pDesc, plane)
      : D3D11_UNORDERED_ACCESS_VIEW_DESC1();
    
    ID3D11UnorderedAccessView1* view = nullptr;

    HRESULT hr = CreateUnorderedAccessView1(pResource,
      pDesc    ? &desc : nullptr,
      ppUAView ? &view : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppUAView = view;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView1(
          ID3D11Resource*                   pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC1* pDesc,
          ID3D11UnorderedAccessView1**      ppUAView) {
    InitReturnPtr(ppUAView);
    
    if (!pResource)
      return E_INVALIDARG;
    
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);

    // The description is optional. If omitted, we'll create
    // a view that covers all subresources of the image.
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 desc;
    
    if (!pDesc) {
      if (FAILED(D3D11UnorderedAccessView::GetDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(D3D11UnorderedAccessView::NormalizeDesc(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    uint32_t plane = D3D11UnorderedAccessView::GetPlaneSlice(&desc);

    if (!CheckResourceViewCompatibility(pResource, D3D11_BIND_UNORDERED_ACCESS, desc.Format, plane)) {
      Logger::err(str::format("D3D11: Cannot create unordered access view:",
        "\n  Resource type:   ", resourceDesc.Dim,
        "\n  Resource usage:  ", resourceDesc.BindFlags,
        "\n  Resource format: ", resourceDesc.Format,
        "\n  View format:     ", desc.Format,
        "\n  View plane:      ", plane));
      return E_INVALIDARG;
    }

    if (!ppUAView)
      return S_FALSE;
    
    try {
      auto uav = new D3D11UnorderedAccessView(this, pResource, &desc);
      m_initializer->InitUavCounter(uav);
      *ppUAView = ref(uav);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(
          ID3D11Resource*                   pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
          ID3D11RenderTargetView**          ppRTView) {
    InitReturnPtr(ppRTView);

    uint32_t plane = GetViewPlaneIndex(pResource, pDesc ? pDesc->Format : DXGI_FORMAT_UNKNOWN);

    D3D11_RENDER_TARGET_VIEW_DESC1 desc = pDesc
      ? D3D11RenderTargetView::PromoteDesc(pDesc, plane)
      : D3D11_RENDER_TARGET_VIEW_DESC1();
    
    ID3D11RenderTargetView1* view = nullptr;

    HRESULT hr = CreateRenderTargetView1(pResource,
      pDesc    ? &desc : nullptr,
      ppRTView ? &view : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppRTView = view;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView1(
          ID3D11Resource*                   pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc,
          ID3D11RenderTargetView1**         ppRTView) {
    InitReturnPtr(ppRTView);

    if (!pResource)
      return E_INVALIDARG;
    
    // DXVK only supports render target views for image resources
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);
    
    if (resourceDesc.Dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      Logger::warn("D3D11: Cannot create render target view for a buffer");
      return S_OK; // It is required to run Battlefield 3 and Battlefield 4.
    }
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_RENDER_TARGET_VIEW_DESC1 desc;
    
    if (!pDesc) {
      if (FAILED(D3D11RenderTargetView::GetDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(D3D11RenderTargetView::NormalizeDesc(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    uint32_t plane = D3D11RenderTargetView::GetPlaneSlice(&desc);

    if (!CheckResourceViewCompatibility(pResource, D3D11_BIND_RENDER_TARGET, desc.Format, plane)) {
      Logger::err(str::format("D3D11: Cannot create render target view:",
        "\n  Resource type:   ", resourceDesc.Dim,
        "\n  Resource usage:  ", resourceDesc.BindFlags,
        "\n  Resource format: ", resourceDesc.Format,
        "\n  View format:     ", desc.Format,
        "\n  View plane:      ", plane));
      return E_INVALIDARG;
    }

    if (!ppRTView)
      return S_FALSE;
    
    try {
      *ppRTView = ref(new D3D11RenderTargetView(this, pResource, &desc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(
          ID3D11Resource*                   pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    InitReturnPtr(ppDepthStencilView);
    
    if (pResource == nullptr)
      return E_INVALIDARG;
    
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);

    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(D3D11DepthStencilView::GetDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(D3D11DepthStencilView::NormalizeDesc(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    if (!CheckResourceViewCompatibility(pResource, D3D11_BIND_DEPTH_STENCIL, desc.Format, 0)) {
      Logger::err(str::format("D3D11: Cannot create depth-stencil view:",
        "\n  Resource type:   ", resourceDesc.Dim,
        "\n  Resource usage:  ", resourceDesc.BindFlags,
        "\n  Resource format: ", resourceDesc.Format,
        "\n  View format:     ", desc.Format));
      return E_INVALIDARG;
    }
    
    if (ppDepthStencilView == nullptr)
      return S_FALSE;
    
    try {
      *ppDepthStencilView = ref(new D3D11DepthStencilView(this, pResource, &desc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
          UINT                        NumElements,
    const void*                       pShaderBytecodeWithInputSignature,
          SIZE_T                      BytecodeLength,
          ID3D11InputLayout**         ppInputLayout) {
    InitReturnPtr(ppInputLayout);

    // This check is somehow even correct, passing null with zero
    // size will always fail but passing non-null with zero size
    // works, provided the shader does not have any actual inputs
    if (!pInputElementDescs)
      return E_INVALIDARG;
    
    try {
      DxbcReader dxbcReader(reinterpret_cast<const char*>(
        pShaderBytecodeWithInputSignature), BytecodeLength);
      DxbcModule dxbcModule(dxbcReader);
      
      const Rc<DxbcIsgn> inputSignature = dxbcModule.isgn();

      uint32_t attrMask = 0;
      uint32_t bindMask = 0;
      uint32_t locationMask = 0;
      uint32_t bindingsDefined = 0;
      
      std::array<DxvkVertexAttribute, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> attrList = { };
      std::array<DxvkVertexBinding,   D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> bindList = { };
      
      for (uint32_t i = 0; i < NumElements; i++) {
        const DxbcSgnEntry* entry = inputSignature->find(
          pInputElementDescs[i].SemanticName,
          pInputElementDescs[i].SemanticIndex, 0);

        // Create vertex input attribute description
        DxvkVertexAttribute attrib;
        attrib.location = entry != nullptr ? entry->registerId : 0;
        attrib.binding  = pInputElementDescs[i].InputSlot;
        attrib.format   = LookupFormat(pInputElementDescs[i].Format, DXGI_VK_FORMAT_MODE_COLOR).Format;
        attrib.offset   = pInputElementDescs[i].AlignedByteOffset;

        // The application may choose to let the implementation
        // generate the exact vertex layout. In that case we'll
        // pack attributes on the same binding in the order they
        // are declared, aligning each attribute to four bytes.
        const DxvkFormatInfo* formatInfo = lookupFormatInfo(attrib.format);
        VkDeviceSize alignment = std::min<VkDeviceSize>(formatInfo->elementSize, 4);

        if (attrib.offset == D3D11_APPEND_ALIGNED_ELEMENT) {
          attrib.offset = 0;

          for (uint32_t j = 1; j <= i; j++) {
            const DxvkVertexAttribute& prev = attrList.at(i - j);

            if (prev.binding == attrib.binding) {
              attrib.offset = align(prev.offset + lookupFormatInfo(prev.format)->elementSize, alignment);
              break;
            }
          }
        } else if (attrib.offset & (alignment - 1))
          return E_INVALIDARG;

        attrList.at(i) = attrib;

        // Create vertex input binding description. The
        // stride is dynamic state in D3D11 and will be
        // set by D3D11DeviceContext::IASetVertexBuffers.
        DxvkVertexBinding binding;
        binding.binding   = pInputElementDescs[i].InputSlot;
        binding.fetchRate = pInputElementDescs[i].InstanceDataStepRate;
        binding.inputRate = pInputElementDescs[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA
          ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        binding.extent    = entry ? uint32_t(attrib.offset + formatInfo->elementSize) : 0u;

        // Check if the binding was already defined. If so, the
        // parameters must be identical (namely, the input rate).
        if (bindingsDefined & (1u << binding.binding)) {
          if (bindList.at(binding.binding).inputRate != binding.inputRate)
            return E_INVALIDARG;

          bindList.at(binding.binding).extent = std::max(
            bindList.at(binding.binding).extent, binding.extent);
        } else {
          bindList.at(binding.binding) = binding;
          bindingsDefined |= 1u << binding.binding;
        }

        if (entry) {
          attrMask |= 1u << i;
          bindMask |= 1u << binding.binding;
          locationMask |= 1u << attrib.location;
        }
      }

      // Ensure that all inputs used by the shader are defined
      for (auto i = inputSignature->begin(); i != inputSignature->end(); i++) {
        bool isBuiltIn = DxbcIsgn::compareSemanticNames(i->semanticName, "sv_instanceid")
                      || DxbcIsgn::compareSemanticNames(i->semanticName, "sv_vertexid");

        if (!isBuiltIn && !(locationMask & (1u << i->registerId)))
          return E_INVALIDARG;
      }

      // Compact the attribute and binding lists to filter
      // out attributes and bindings not used by the shader
      uint32_t attrCount = CompactSparseList(attrList.data(), attrMask);
      uint32_t bindCount = CompactSparseList(bindList.data(), bindMask);

      if (!ppInputLayout)
        return S_FALSE;

      *ppInputLayout = ref(
        new D3D11InputLayout(this,
          attrCount, attrList.data(),
          bindCount, bindList.data()));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11VertexShader**        ppVertexShader) {
    InitReturnPtr(ppVertexShader);
    D3D11CommonShader module;

    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_VERTEX_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage,
      &moduleInfo);
    
    if (FAILED(hr))
      return hr;
    
    if (!ppVertexShader)
      return S_FALSE;
    
    *ppVertexShader = ref(new D3D11VertexShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11GeometryShader**      ppGeometryShader) {
    InitReturnPtr(ppGeometryShader);
    D3D11CommonShader module;
    
    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_GEOMETRY_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage,
      &moduleInfo);

    if (FAILED(hr))
      return hr;
    
    if (!ppGeometryShader)
      return S_FALSE;
    
    *ppGeometryShader = ref(new D3D11GeometryShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
          UINT                        NumEntries,
    const UINT*                       pBufferStrides,
          UINT                        NumStrides,
          UINT                        RasterizedStream,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11GeometryShader**      ppGeometryShader) {
    InitReturnPtr(ppGeometryShader);
    D3D11CommonShader module;

    if (!m_dxvkDevice->features().extTransformFeedback.transformFeedback)
      return DXGI_ERROR_INVALID_CALL;

    // Zero-init some counterss so that we can increment
    // them while walking over the stream output entries
    DxbcXfbInfo xfb = { };

    for (uint32_t i = 0; i < NumEntries; i++) {
      const D3D11_SO_DECLARATION_ENTRY* so = &pSODeclaration[i];

      if (so->OutputSlot >= D3D11_SO_BUFFER_SLOT_COUNT)
        return E_INVALIDARG;

      if (so->SemanticName != nullptr) {
        if (so->Stream >= D3D11_SO_BUFFER_SLOT_COUNT
         || so->StartComponent >= 4
         || so->ComponentCount <  1
         || so->ComponentCount >  4)
          return E_INVALIDARG;
        
        DxbcXfbEntry* entry = &xfb.entries[xfb.entryCount++];
        entry->semanticName   = so->SemanticName;
        entry->semanticIndex  = so->SemanticIndex;
        entry->componentIndex = so->StartComponent;
        entry->componentCount = so->ComponentCount;
        entry->streamId       = so->Stream;
        entry->bufferId       = so->OutputSlot;
        entry->offset         = xfb.strides[so->OutputSlot];
      }

      xfb.strides[so->OutputSlot] += so->ComponentCount * sizeof(uint32_t);
    }
    
    // If necessary, override the buffer strides
    for (uint32_t i = 0; i < NumStrides; i++)
      xfb.strides[i] = pBufferStrides[i];

    // Set stream to rasterize, if any
    xfb.rasterizedStream = -1;
    
    if (RasterizedStream != D3D11_SO_NO_RASTERIZED_STREAM)
      Logger::err("D3D11: CreateGeometryShaderWithStreamOutput: Rasterized stream not supported");
    
    // Compute hash from both the xfb info and the source
    // code, because both influence the generated code
    DxbcXfbInfo hashXfb = xfb;

    std::vector<Sha1Data> chunks = {{
      { pShaderBytecode, BytecodeLength  },
      { &hashXfb,        sizeof(hashXfb) },
    }};

    for (uint32_t i = 0; i < hashXfb.entryCount; i++) {
      const char* semantic = hashXfb.entries[i].semanticName;

      if (semantic) {
        chunks.push_back({ semantic, std::strlen(semantic) });
        hashXfb.entries[i].semanticName = nullptr;
      }
    }

    Sha1Hash hash = Sha1Hash::compute(chunks.size(), chunks.data());
    
    // Create the actual shader module
    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = &xfb;
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_GEOMETRY_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage,
      &moduleInfo);

    if (FAILED(hr))
      return E_INVALIDARG;
    
    if (!ppGeometryShader)
      return S_FALSE;
    
    *ppGeometryShader = ref(new D3D11GeometryShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11PixelShader**         ppPixelShader) {
    InitReturnPtr(ppPixelShader);
    D3D11CommonShader module;
    
    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    

    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_FRAGMENT_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage,
      &moduleInfo);

    if (FAILED(hr))
      return hr;
    
    if (!ppPixelShader)
      return S_FALSE;
    
    *ppPixelShader = ref(new D3D11PixelShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11HullShader**          ppHullShader) {
    InitReturnPtr(ppHullShader);
    D3D11CommonShader module;
    
    DxbcTessInfo tessInfo;
    tessInfo.maxTessFactor = float(m_d3d11Options.maxTessFactor);

    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    if (tessInfo.maxTessFactor >= 8.0f)
      moduleInfo.tess = &tessInfo;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage, &moduleInfo);

    if (FAILED(hr))
      return hr;
    
    if (!ppHullShader)
      return S_FALSE;
    
    *ppHullShader = ref(new D3D11HullShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11DomainShader**        ppDomainShader) {
    InitReturnPtr(ppDomainShader);
    D3D11CommonShader module;
    
    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage, &moduleInfo);

    if (FAILED(hr))
      return hr;
    
    if (ppDomainShader == nullptr)
      return S_FALSE;
    
    *ppDomainShader = ref(new D3D11DomainShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11ComputeShader**       ppComputeShader) {
    InitReturnPtr(ppComputeShader);
    D3D11CommonShader module;
    
    DxbcModuleInfo moduleInfo;
    moduleInfo.options = m_dxbcOptions;
    moduleInfo.tess    = nullptr;
    moduleInfo.xfb     = nullptr;

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);
    
    HRESULT hr = CreateShaderModule(&module,
      DxvkShaderKey(VK_SHADER_STAGE_COMPUTE_BIT, hash),
      pShaderBytecode, BytecodeLength, pClassLinkage,
      &moduleInfo);

    if (FAILED(hr))
      return hr;
    
    if (!ppComputeShader)
      return S_FALSE;
    
    *ppComputeShader = ref(new D3D11ComputeShader(this, module));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) {
    *ppLinkage = ref(new D3D11ClassLinkage(this));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC*           pBlendStateDesc,
          ID3D11BlendState**          ppBlendState) {
    InitReturnPtr(ppBlendState);

    if (!pBlendStateDesc)
      return E_INVALIDARG;
    
    D3D11_BLEND_DESC1 desc = D3D11BlendState::PromoteDesc(pBlendStateDesc);
    
    if (FAILED(D3D11BlendState::NormalizeDesc(&desc)))
      return E_INVALIDARG;
    
    if (ppBlendState != nullptr) {
      *ppBlendState = m_bsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState1(
    const D3D11_BLEND_DESC1*          pBlendStateDesc, 
          ID3D11BlendState1**         ppBlendState) {
    InitReturnPtr(ppBlendState);
    
    if (!pBlendStateDesc)
      return E_INVALIDARG;

    D3D11_BLEND_DESC1 desc = *pBlendStateDesc;
    
    if (FAILED(D3D11BlendState::NormalizeDesc(&desc)))
      return E_INVALIDARG;
    
    if (ppBlendState != nullptr) {
      *ppBlendState = m_bsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
          ID3D11DepthStencilState**   ppDepthStencilState) {
    InitReturnPtr(ppDepthStencilState);
    
    if (!pDepthStencilDesc)
      return E_INVALIDARG;

    D3D11_DEPTH_STENCIL_DESC desc = *pDepthStencilDesc;
    
    if (FAILED(D3D11DepthStencilState::NormalizeDesc(&desc)))
      return E_INVALIDARG;
    
    if (ppDepthStencilState != nullptr) {
      *ppDepthStencilState = m_dsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC*      pRasterizerDesc,
          ID3D11RasterizerState**     ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);

    if (!pRasterizerDesc)
      return E_INVALIDARG;

    D3D11_RASTERIZER_DESC2 desc = D3D11RasterizerState::PromoteDesc(pRasterizerDesc);
    
    if (FAILED(D3D11RasterizerState::NormalizeDesc(&desc)))
      return E_INVALIDARG;
    
    if (!ppRasterizerState)
      return S_FALSE;
    
    *ppRasterizerState = m_rsStateObjects.Create(this, desc);
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1*     pRasterizerDesc, 
          ID3D11RasterizerState1**    ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);
    
    if (!pRasterizerDesc)
      return E_INVALIDARG;

    D3D11_RASTERIZER_DESC2 desc = D3D11RasterizerState::PromoteDesc(pRasterizerDesc);
    
    if (FAILED(D3D11RasterizerState::NormalizeDesc(&desc)))
      return E_INVALIDARG;
    
    if (!ppRasterizerState)
      return S_FALSE;
    
    *ppRasterizerState = m_rsStateObjects.Create(this, desc);
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateRasterizerState2(
    const D3D11_RASTERIZER_DESC2*     pRasterizerDesc, 
          ID3D11RasterizerState2**    ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);
    
    if (!pRasterizerDesc)
      return E_INVALIDARG;

    D3D11_RASTERIZER_DESC2 desc = *pRasterizerDesc;
    
    if (FAILED(D3D11RasterizerState::NormalizeDesc(&desc)))
      return E_INVALIDARG;

    if (desc.ConservativeRaster != D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF
     && !m_deviceFeatures.GetConservativeRasterizationTier())
      return E_INVALIDARG;

    if (!ppRasterizerState)
      return S_FALSE;
    
    *ppRasterizerState = m_rsStateObjects.Create(this, desc);
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateSamplerState(
    const D3D11_SAMPLER_DESC*         pSamplerDesc,
          ID3D11SamplerState**        ppSamplerState) {
    InitReturnPtr(ppSamplerState);

    if (pSamplerDesc == nullptr)
      return E_INVALIDARG;

    D3D11_SAMPLER_DESC desc = *pSamplerDesc;
    
    if (FAILED(D3D11SamplerState::NormalizeDesc(&desc)))
      return E_INVALIDARG;

    D3D11_TILED_RESOURCES_TIER tiledResourcesTier = m_deviceFeatures.GetTiledResourcesTier();

    if (IsMinMaxFilter(desc.Filter) && tiledResourcesTier < D3D11_TILED_RESOURCES_TIER_2)
      return E_INVALIDARG;

    if (!ppSamplerState)
      return S_FALSE;
    
    try {
      *ppSamplerState = m_samplerObjects.Create(this, desc);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery(
    const D3D11_QUERY_DESC*           pQueryDesc,
          ID3D11Query**               ppQuery) {
    InitReturnPtr(ppQuery);

    if (!pQueryDesc)
      return E_INVALIDARG;
    
    D3D11_QUERY_DESC1 desc;
    desc.Query       = pQueryDesc->Query;
    desc.MiscFlags   = pQueryDesc->MiscFlags;
    desc.ContextType = D3D11_CONTEXT_TYPE_ALL;

    ID3D11Query1* query = nullptr;
    HRESULT hr = CreateQuery1(&desc, ppQuery ? &query : nullptr);

    if (hr != S_OK)
      return hr;

    *ppQuery = query;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery1(
    const D3D11_QUERY_DESC1*          pQueryDesc,
          ID3D11Query1**              ppQuery) {
    InitReturnPtr(ppQuery);

    if (!pQueryDesc)
      return E_INVALIDARG;
    
    HRESULT hr = D3D11Query::ValidateDesc(pQueryDesc);

    if (FAILED(hr))
      return hr;
    
    if (!ppQuery)
      return S_FALSE;
    
    try {
      *ppQuery = ref(new D3D11Query(this, *pQueryDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }

  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC*           pPredicateDesc,
          ID3D11Predicate**           ppPredicate) {
    InitReturnPtr(ppPredicate);
    
    if (!pPredicateDesc)
      return E_INVALIDARG;

    D3D11_QUERY_DESC1 desc;
    desc.Query       = pPredicateDesc->Query;
    desc.MiscFlags   = pPredicateDesc->MiscFlags;
    desc.ContextType = D3D11_CONTEXT_TYPE_ALL;

    if (desc.Query != D3D11_QUERY_OCCLUSION_PREDICATE) {
      Logger::warn(str::format("D3D11: Unhandled predicate type: ", pPredicateDesc->Query));
      return E_INVALIDARG;
    }
    
    if (!ppPredicate)
      return S_FALSE;
    
    try {
      *ppPredicate = D3D11Query::AsPredicate(
        ref(new D3D11Query(this, desc)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC*         pCounterDesc,
          ID3D11Counter**             ppCounter) {
    InitReturnPtr(ppCounter);
    
    Logger::err(str::format("D3D11: Unsupported counter: ", pCounterDesc->Counter));
    return E_INVALIDARG;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext(
          UINT                        ContextFlags,
          ID3D11DeviceContext**       ppDeferredContext) {
    *ppDeferredContext = ref(new D3D11DeferredContext(this, m_dxvkDevice, ContextFlags));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext1(
          UINT                        ContextFlags, 
          ID3D11DeviceContext1**      ppDeferredContext) {
    *ppDeferredContext = ref(new D3D11DeferredContext(this, m_dxvkDevice, ContextFlags));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext2(
          UINT                        ContextFlags, 
          ID3D11DeviceContext2**      ppDeferredContext) {
    *ppDeferredContext = ref(new D3D11DeferredContext(this, m_dxvkDevice, ContextFlags));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext3(
          UINT                        ContextFlags, 
          ID3D11DeviceContext3**      ppDeferredContext) {
    *ppDeferredContext = ref(new D3D11DeferredContext(this, m_dxvkDevice, ContextFlags));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeviceContextState(
          UINT                        Flags, 
    const D3D_FEATURE_LEVEL*          pFeatureLevels, 
          UINT                        FeatureLevels, 
          UINT                        SDKVersion, 
          REFIID                      EmulatedInterface, 
          D3D_FEATURE_LEVEL*          pChosenFeatureLevel, 
          ID3DDeviceContextState**    ppContextState) {
    InitReturnPtr(ppContextState);

    if (!pFeatureLevels || !FeatureLevels)
      return E_INVALIDARG;

    if (EmulatedInterface != __uuidof(ID3D10Device)
     && EmulatedInterface != __uuidof(ID3D10Device1)
     && EmulatedInterface != __uuidof(ID3D11Device)
     && EmulatedInterface != __uuidof(ID3D11Device1))
      return E_INVALIDARG;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL();

    for (uint32_t flId = 0; flId < FeatureLevels; flId++) {
      if (pFeatureLevels[flId] <= m_maxFeatureLevel) {
        featureLevel = pFeatureLevels[flId];
        break;
      }
    }

    if (!featureLevel)
      return E_INVALIDARG;

    if (m_featureLevel < featureLevel) {
      m_featureLevel = featureLevel;
      m_deviceFeatures = D3D11DeviceFeatures(
        m_dxvkDevice->instance(),
        m_dxvkDevice->adapter(),
        m_d3d11Options, m_featureLevel);
    }

    if (pChosenFeatureLevel)
      *pChosenFeatureLevel = featureLevel;

    if (!ppContextState)
      return S_FALSE;

    *ppContextState = ref(new D3D11DeviceContextState(this));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateFence(
          UINT64                      InitialValue,
          D3D11_FENCE_FLAG            Flags,
          REFIID                      riid,
          void**                      ppFence) {
    InitReturnPtr(ppFence);

    try {
      Com<D3D11Fence> fence = new D3D11Fence(this, InitialValue, Flags, INVALID_HANDLE_VALUE);
      return fence->QueryInterface(riid, ppFence);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  void STDMETHODCALLTYPE D3D11Device::ReadFromSubresource(
          void*                       pDstData,
          UINT                        DstRowPitch,
          UINT                        DstDepthPitch,
          ID3D11Resource*             pSrcResource,
          UINT                        SrcSubresource,
    const D3D11_BOX*                  pSrcBox) {
    auto texture = GetCommonTexture(pSrcResource);

    if (!texture)
      return;

    if (texture->Desc()->Usage != D3D11_USAGE_DEFAULT
     || texture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_NONE
     || texture->CountSubresources() <= SrcSubresource)
      return;

    D3D11_MAP map = texture->GetMapType(SrcSubresource);

    if (map != D3D11_MAP_READ
     && map != D3D11_MAP_READ_WRITE)
      return;

    CopySubresourceData(
      pDstData, DstRowPitch, DstDepthPitch,
      texture, SrcSubresource, pSrcBox);
  }


  void STDMETHODCALLTYPE D3D11Device::WriteToSubresource(
          ID3D11Resource*             pDstResource,
          UINT                        DstSubresource,
    const D3D11_BOX*                  pDstBox,
    const void*                       pSrcData,
          UINT                        SrcRowPitch,
          UINT                        SrcDepthPitch) {
    auto texture = GetCommonTexture(pDstResource);

    if (!texture)
      return;

    if (texture->Desc()->Usage != D3D11_USAGE_DEFAULT
     || texture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_NONE
     || texture->CountSubresources() <= DstSubresource)
      return;

    D3D11_MAP map = texture->GetMapType(DstSubresource);

    if (map != D3D11_MAP_WRITE
     && map != D3D11_MAP_WRITE_NO_OVERWRITE
     && map != D3D11_MAP_READ_WRITE)
      return;

    CopySubresourceData(
      pSrcData, SrcRowPitch, SrcRowPitch,
      texture, DstSubresource, pDstBox);
  }


  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    return OpenSharedResourceGeneric<true>(
      hResource, ReturnedInterface, ppResource);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource1(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    return OpenSharedResourceGeneric<false>(
      hResource, ReturnedInterface, ppResource);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResourceByName(
          LPCWSTR     lpName, 
          DWORD       dwDesiredAccess, 
          REFIID      returnedInterface, 
          void**      ppResource) {
    InitReturnPtr(ppResource);
    
    Logger::err("D3D11Device::OpenSharedResourceByName: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedFence(
          HANDLE      hFence,
          REFIID      ReturnedInterface,
          void**      ppFence) {
    InitReturnPtr(ppFence);

    if (ppFence == nullptr)
      return S_FALSE;

    try {
      Com<D3D11Fence> fence = new D3D11Fence(this, 0, D3D11_FENCE_FLAG_SHARED, hFence);
      return fence->QueryInterface(ReturnedInterface, ppFence);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11Device::CheckFormatSupport(
          DXGI_FORMAT Format,
          UINT*       pFormatSupport) {
    return GetFormatSupportFlags(Format, pFormatSupport, nullptr);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels(
          DXGI_FORMAT Format,
          UINT        SampleCount,
          UINT*       pNumQualityLevels) {
    return CheckMultisampleQualityLevels1(Format, SampleCount, 0, pNumQualityLevels);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels1(
          DXGI_FORMAT Format,
          UINT        SampleCount,
          UINT        Flags,
          UINT*       pNumQualityLevels) {
    // There are many error conditions, so we'll just assume
    // that we will fail and return a non-zero value in case
    // the device does actually support the format.
    if (!pNumQualityLevels)
      return E_INVALIDARG;
    
    // We don't support tiled resources, but it's unclear what
    // we are supposed to return in this case. Be conservative.
    if (Flags) {
      *pNumQualityLevels = 0;
      return E_FAIL;
    }
    
    // For some reason, we can query DXGI_FORMAT_UNKNOWN
    if (Format == DXGI_FORMAT_UNKNOWN) {
      *pNumQualityLevels = SampleCount == 1 ? 1 : 0;
      return SampleCount ? S_OK : E_FAIL;
    }
    
    // All other unknown formats should result in an error return.
    VkFormat format = LookupFormat(Format, DXGI_VK_FORMAT_MODE_ANY).Format;

    if (format == VK_FORMAT_UNDEFINED)
      return E_INVALIDARG;
    
    // Zero-init now, leave value undefined otherwise.
    // This does actually match native D3D11 behaviour.
    *pNumQualityLevels = 0;

    // Non-power of two sample counts are not supported, but querying
    // support for them is legal, so we return zero quality levels.
    VkSampleCountFlagBits sampleCountFlag = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(DecodeSampleCount(SampleCount, &sampleCountFlag)))
      return SampleCount && SampleCount <= 32 ? S_OK : E_FAIL;

    // Get image create flags depending on function arguments
    VkImageCreateFlags flags = 0;

    if (Flags & D3D11_CHECK_MULTISAMPLE_QUALITY_LEVELS_TILED_RESOURCE) {
      flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT
            |  VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
            |  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
    }

    // Check if the device supports the given combination of format
    // and sample count. D3D exposes the opaque concept of quality
    // levels to the application, we'll just define one such level.
    DxvkFormatQuery formatQuery = { };
    formatQuery.format = format;
    formatQuery.type = VK_IMAGE_TYPE_2D;
    formatQuery.tiling = VK_IMAGE_TILING_OPTIMAL;
    formatQuery.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    formatQuery.flags = flags;

    auto properties = m_dxvkDevice->getFormatLimits(formatQuery);

    if (properties && (properties->sampleCounts & sampleCountFlag))
      *pNumQualityLevels = 1;
    return S_OK;
  }


  void STDMETHODCALLTYPE D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) {
    // We basically don't support counters
    pCounterInfo->LastDeviceDependentCounter  = D3D11_COUNTER(0);
    pCounterInfo->NumSimultaneousCounters     = 0;
    pCounterInfo->NumDetectableParallelUnits  = 0;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckCounter(
    const D3D11_COUNTER_DESC* pDesc,
          D3D11_COUNTER_TYPE* pType,
          UINT*               pActiveCounters,
          LPSTR               szName,
          UINT*               pNameLength,
          LPSTR               szUnits,
          UINT*               pUnitsLength,
          LPSTR               szDescription,
          UINT*               pDescriptionLength) {
    Logger::err("D3D11: Counters not supported");
    return E_INVALIDARG;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(
          D3D11_FEATURE Feature,
          void*         pFeatureSupportData,
          UINT          FeatureSupportDataSize) {
    switch (Feature) {
      // Format support queries are special in that they use in-out
      // structs, and we need the Vulkan device to query them at all
      case D3D11_FEATURE_FORMAT_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        return GetFormatSupportFlags(info->InFormat, &info->OutFormatSupport, nullptr);
      } return S_OK;

      case D3D11_FEATURE_FORMAT_SUPPORT2: {
        auto info = static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT2*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        return GetFormatSupportFlags(info->InFormat, nullptr, &info->OutFormatSupport2);
      } return S_OK;

      default:
        // For everything else, we can use the device feature struct
        // that we already initialized during device creation.
        return m_deviceFeatures.GetFeatureData(Feature, FeatureSupportDataSize, pFeatureSupportData);
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(
          REFGUID guid, UINT* pDataSize, void* pData) {
    return m_container->GetPrivateData(guid, pDataSize, pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(
          REFGUID guid, UINT DataSize, const void* pData) {
    return m_container->SetPrivateData(guid, DataSize, pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateDataInterface(
          REFGUID guid, const IUnknown* pData) {
    return m_container->SetPrivateDataInterface(guid, pData);
  }
  
  
  D3D_FEATURE_LEVEL STDMETHODCALLTYPE D3D11Device::GetFeatureLevel() {
    return m_featureLevel;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Device::GetCreationFlags() {
    return m_featureFlags;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason() {
    VkResult status = m_dxvkDevice->getDeviceStatus();

    switch (status) {
      case VK_SUCCESS: return S_OK;
      default:         return DXGI_ERROR_DEVICE_RESET;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) {
    *ppImmediateContext = m_context.ref();
  }


  void STDMETHODCALLTYPE D3D11Device::GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext) {
    *ppImmediateContext = m_context.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Device::GetImmediateContext2(ID3D11DeviceContext2** ppImmediateContext) {
    *ppImmediateContext = m_context.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Device::GetImmediateContext3(ID3D11DeviceContext3** ppImmediateContext) {
    *ppImmediateContext = m_context.ref();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetExceptionMode(UINT RaiseFlags) {
    Logger::err("D3D11Device::SetExceptionMode: Not implemented");
    return E_NOTIMPL;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Device::GetExceptionMode() {
    Logger::err("D3D11Device::GetExceptionMode: Not implemented");
    return 0;
  }


  void STDMETHODCALLTYPE D3D11Device::GetResourceTiling(
          ID3D11Resource*           pTiledResource,
          UINT*                     pNumTilesForEntireResource,
          D3D11_PACKED_MIP_DESC*    pPackedMipDesc,
          D3D11_TILE_SHAPE*         pStandardTileShapeForNonPackedMips,
          UINT*                     pNumSubresourceTilings,
          UINT                      FirstSubresourceTilingToGet,
          D3D11_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    D3D11_COMMON_RESOURCE_DESC desc = { };
    GetCommonResourceDesc(pTiledResource, &desc);

    if (!(desc.MiscFlags & D3D11_RESOURCE_MISC_TILED)) {
      if (pNumTilesForEntireResource)
        *pNumTilesForEntireResource = 0;

      if (pPackedMipDesc)
        *pPackedMipDesc = D3D11_PACKED_MIP_DESC();

      if (pStandardTileShapeForNonPackedMips)
        *pStandardTileShapeForNonPackedMips = D3D11_TILE_SHAPE();

      if (pNumSubresourceTilings) {
        if (pSubresourceTilingsForNonPackedMips) {
          for (uint32_t i = 0; i < *pNumSubresourceTilings; i++)
            pSubresourceTilingsForNonPackedMips[i] = D3D11_SUBRESOURCE_TILING();
        }

        *pNumSubresourceTilings = 0;
      }
    } else {
      DxvkSparsePageTable* sparseInfo = nullptr;
      uint32_t mipCount = 0;

      if (desc.Dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
        Rc<DxvkBuffer> buffer = static_cast<D3D11Buffer*>(pTiledResource)->GetBuffer();
        sparseInfo = buffer->getSparsePageTable();
      } else {
        Rc<DxvkImage> image = GetCommonTexture(pTiledResource)->GetImage();
        sparseInfo = image->getSparsePageTable();
        mipCount = image->info().mipLevels;
      }

      if (pNumTilesForEntireResource)
        *pNumTilesForEntireResource = sparseInfo->getPageCount();

      if (pPackedMipDesc) {
        auto properties = sparseInfo->getProperties();

        if (properties.mipTailSize) {
          pPackedMipDesc->NumStandardMips = properties.pagedMipCount;
          pPackedMipDesc->NumPackedMips = mipCount - properties.pagedMipCount;
          pPackedMipDesc->NumTilesForPackedMips = sparseInfo->getPageCount() - properties.mipTailPageIndex;
          pPackedMipDesc->StartTileIndexInOverallResource = properties.mipTailPageIndex;
        } else {
          pPackedMipDesc->NumStandardMips = mipCount;
          pPackedMipDesc->NumPackedMips = 0;
          pPackedMipDesc->NumTilesForPackedMips = 0;
          pPackedMipDesc->StartTileIndexInOverallResource = 0;
        }
      }

      if (pStandardTileShapeForNonPackedMips) {
        auto properties = sparseInfo->getProperties();
        pStandardTileShapeForNonPackedMips->WidthInTexels = properties.pageRegionExtent.width;
        pStandardTileShapeForNonPackedMips->HeightInTexels = properties.pageRegionExtent.height;
        pStandardTileShapeForNonPackedMips->DepthInTexels = properties.pageRegionExtent.depth;
      }

      if (pNumSubresourceTilings) {
        uint32_t subresourceCount = sparseInfo->getSubresourceCount();
        uint32_t tilingCount = subresourceCount - std::min(FirstSubresourceTilingToGet, subresourceCount);
        tilingCount = std::min(tilingCount, *pNumSubresourceTilings);

        for (uint32_t i = 0; i < tilingCount; i++) {
          auto subresourceInfo = sparseInfo->getSubresourceProperties(FirstSubresourceTilingToGet + i);
          auto dstInfo = &pSubresourceTilingsForNonPackedMips[i];

          if (subresourceInfo.isMipTail) {
            dstInfo->WidthInTiles = 0u;
            dstInfo->HeightInTiles = 0u;
            dstInfo->DepthInTiles = 0u;
            dstInfo->StartTileIndexInOverallResource = D3D11_PACKED_TILE;
          } else {
            dstInfo->WidthInTiles = subresourceInfo.pageCount.width;
            dstInfo->HeightInTiles = subresourceInfo.pageCount.height;
            dstInfo->DepthInTiles = subresourceInfo.pageCount.depth;
            dstInfo->StartTileIndexInOverallResource = subresourceInfo.pageIndex;
          }
        }

        *pNumSubresourceTilings = tilingCount;
      }
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::RegisterDeviceRemovedEvent(
          HANDLE                    hEvent,
          DWORD*                    pdwCookie) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11Device::RegisterDeviceRemovedEvent: Not implemented");

    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11Device::UnregisterDeviceRemoved(
          DWORD                     dwCookie) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11Device::UnregisterDeviceRemovedEvent: Not implemented");
  }


  DXGI_VK_FORMAT_INFO D3D11Device::LookupFormat(
          DXGI_FORMAT           Format,
          DXGI_VK_FORMAT_MODE   Mode) const {
    return m_d3d11Formats.GetFormatInfo(Format, Mode);
  }
  
  
  DXGI_VK_FORMAT_INFO D3D11Device::LookupPackedFormat(
          DXGI_FORMAT           Format,
          DXGI_VK_FORMAT_MODE   Mode) const {
    return m_d3d11Formats.GetPackedFormatInfo(Format, Mode);
  }
  
  
  DXGI_VK_FORMAT_FAMILY D3D11Device::LookupFamily(
          DXGI_FORMAT           Format,
          DXGI_VK_FORMAT_MODE   Mode) const {
    return m_d3d11Formats.GetFormatFamily(Format, Mode);
  }


  bool D3D11Device::Is11on12Device() const {
    return m_container->Is11on12Device();
  }
  
  
  void D3D11Device::FlushInitContext() {
    m_initializer->Flush();
  }
  
  
  D3D_FEATURE_LEVEL D3D11Device::GetMaxFeatureLevel(
    const Rc<DxvkInstance>& Instance,
    const Rc<DxvkAdapter>&  Adapter) {
    // Check whether baseline features are supported by the device    
    DxvkDeviceFeatures features = GetDeviceFeatures(Adapter);
    
    if (!Adapter->checkFeatureSupport(features))
      return D3D_FEATURE_LEVEL();

    // The feature level override always takes precedence
    static const std::array<std::pair<std::string, D3D_FEATURE_LEVEL>, 9> s_featureLevels = {{
      { "12_1", D3D_FEATURE_LEVEL_12_1 },
      { "12_0", D3D_FEATURE_LEVEL_12_0 },
      { "11_1", D3D_FEATURE_LEVEL_11_1 },
      { "11_0", D3D_FEATURE_LEVEL_11_0 },
      { "10_1", D3D_FEATURE_LEVEL_10_1 },
      { "10_0", D3D_FEATURE_LEVEL_10_0 },
      { "9_3",  D3D_FEATURE_LEVEL_9_3  },
      { "9_2",  D3D_FEATURE_LEVEL_9_2  },
      { "9_1",  D3D_FEATURE_LEVEL_9_1  },
    }};
    
    std::string maxLevel = Instance->config().getOption<std::string>("d3d11.maxFeatureLevel");

    auto entry = std::find_if(s_featureLevels.begin(), s_featureLevels.end(),
      [&] (const std::pair<std::string, D3D_FEATURE_LEVEL>& pair) {
        return pair.first == maxLevel;
      });

    if (entry != s_featureLevels.end())
      return entry->second;

    // Otherwise, check the actually available device features
    return D3D11DeviceFeatures::GetMaxFeatureLevel(Instance, Adapter);
  }
  
  
  DxvkDeviceFeatures D3D11Device::GetDeviceFeatures(
    const Rc<DxvkAdapter>&  Adapter) {
    DxvkDeviceFeatures supported = Adapter->features();
    DxvkDeviceFeatures enabled   = {};

    // Required for feature level 10_1
    enabled.core.features.depthBiasClamp                          = VK_TRUE;
    enabled.core.features.depthClamp                              = VK_TRUE;
    enabled.core.features.dualSrcBlend                            = VK_TRUE;
    enabled.core.features.fillModeNonSolid                        = VK_TRUE;
    enabled.core.features.fullDrawIndexUint32                     = VK_TRUE;
    enabled.core.features.geometryShader                          = VK_TRUE;
    enabled.core.features.imageCubeArray                          = VK_TRUE;
    enabled.core.features.independentBlend                        = VK_TRUE;
    enabled.core.features.multiViewport                           = VK_TRUE;
    enabled.core.features.occlusionQueryPrecise                   = VK_TRUE;
    enabled.core.features.pipelineStatisticsQuery                 = supported.core.features.pipelineStatisticsQuery;
    enabled.core.features.sampleRateShading                       = VK_TRUE;
    enabled.core.features.samplerAnisotropy                       = supported.core.features.samplerAnisotropy;
    enabled.core.features.shaderClipDistance                      = VK_TRUE;
    enabled.core.features.shaderCullDistance                      = VK_TRUE;
    enabled.core.features.shaderImageGatherExtended               = VK_TRUE;
    enabled.core.features.textureCompressionBC                    = VK_TRUE;

    enabled.vk11.shaderDrawParameters                             = VK_TRUE;

    enabled.vk12.samplerMirrorClampToEdge                         = VK_TRUE;

    enabled.vk13.shaderDemoteToHelperInvocation                   = VK_TRUE;

    enabled.extCustomBorderColor.customBorderColors               = supported.extCustomBorderColor.customBorderColorWithoutFormat;
    enabled.extCustomBorderColor.customBorderColorWithoutFormat   = supported.extCustomBorderColor.customBorderColorWithoutFormat;

    enabled.extTransformFeedback.transformFeedback                = VK_TRUE;
    enabled.extTransformFeedback.geometryStreams                  = VK_TRUE;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor      = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor  = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;

    // Required for Feature Level 11_0
    enabled.core.features.drawIndirectFirstInstance               = supported.core.features.drawIndirectFirstInstance;
    enabled.core.features.fragmentStoresAndAtomics                = supported.core.features.fragmentStoresAndAtomics;
    enabled.core.features.multiDrawIndirect                       = supported.core.features.multiDrawIndirect;
    enabled.core.features.tessellationShader                      = supported.core.features.tessellationShader;

    // Required for Feature Level 11_1
    enabled.core.features.logicOp                                 = supported.core.features.logicOp;
    enabled.core.features.vertexPipelineStoresAndAtomics          = supported.core.features.vertexPipelineStoresAndAtomics;

    // Required for Feature Level 12_0
    enabled.core.features.sparseBinding                           = supported.core.features.sparseBinding;
    enabled.core.features.sparseResidencyBuffer                   = supported.core.features.sparseResidencyBuffer;
    enabled.core.features.sparseResidencyImage2D                  = supported.core.features.sparseResidencyImage2D;
    enabled.core.features.sparseResidencyImage3D                  = supported.core.features.sparseResidencyImage3D;
    enabled.core.features.sparseResidency2Samples                 = supported.core.features.sparseResidency2Samples;
    enabled.core.features.sparseResidency4Samples                 = supported.core.features.sparseResidency4Samples;
    enabled.core.features.sparseResidency8Samples                 = supported.core.features.sparseResidency8Samples;
    enabled.core.features.sparseResidency16Samples                = supported.core.features.sparseResidency16Samples;
    enabled.core.features.sparseResidencyAliased                  = supported.core.features.sparseResidencyAliased;
    enabled.core.features.shaderResourceResidency                 = supported.core.features.shaderResourceResidency;
    enabled.core.features.shaderResourceMinLod                    = supported.core.features.shaderResourceMinLod;
    enabled.vk12.samplerFilterMinmax                              = supported.vk12.samplerFilterMinmax;

    // Required for Feature Level 12_1
    enabled.extFragmentShaderInterlock.fragmentShaderSampleInterlock = supported.extFragmentShaderInterlock.fragmentShaderSampleInterlock;
    enabled.extFragmentShaderInterlock.fragmentShaderPixelInterlock  = supported.extFragmentShaderInterlock.fragmentShaderPixelInterlock;

    // Optional in any feature level
    enabled.core.features.depthBounds                             = supported.core.features.depthBounds;
    enabled.core.features.shaderFloat64                           = supported.core.features.shaderFloat64;
    enabled.core.features.shaderInt64                             = supported.core.features.shaderInt64;

    // Depth bias control
    enabled.extDepthBiasControl.depthBiasControl                                = supported.extDepthBiasControl.depthBiasControl;
    enabled.extDepthBiasControl.depthBiasExact                                  = supported.extDepthBiasControl.depthBiasExact;
    enabled.extDepthBiasControl.leastRepresentableValueForceUnormRepresentation = supported.extDepthBiasControl.leastRepresentableValueForceUnormRepresentation;

    return enabled;
  }
  
  
  HRESULT D3D11Device::CreateShaderModule(
          D3D11CommonShader*      pShaderModule,
          DxvkShaderKey           ShaderKey,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
          ID3D11ClassLinkage*     pClassLinkage,
    const DxbcModuleInfo*         pModuleInfo) {
    if (pClassLinkage != nullptr)
      Logger::warn("D3D11Device::CreateShaderModule: Class linkage not supported");

    D3D11CommonShader commonShader;

    HRESULT hr = m_shaderModules.GetShaderModule(this,
      &ShaderKey, pModuleInfo, pShaderBytecode, BytecodeLength,
      &commonShader);

    if (FAILED(hr))
      return hr;

    auto shader = commonShader.GetShader();

    if (shader->flags().test(DxvkShaderFlag::ExportsStencilRef)
     && !m_dxvkDevice->features().extShaderStencilExport)
      return E_INVALIDARG;

    if (shader->flags().test(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage)
     && (!m_dxvkDevice->features().vk12.shaderOutputViewportIndex
      || !m_dxvkDevice->features().vk12.shaderOutputLayer))
      return E_INVALIDARG;

    if (shader->flags().test(DxvkShaderFlag::UsesSparseResidency)
     && !m_dxvkDevice->features().core.features.shaderResourceResidency)
      return E_INVALIDARG;

    if (shader->flags().test(DxvkShaderFlag::UsesFragmentCoverage)
     && !m_dxvkDevice->properties().extConservativeRasterization.fullyCoveredFragmentShaderInputVariable)
      return E_INVALIDARG;

    *pShaderModule = std::move(commonShader);
    return S_OK;
  }


  HRESULT D3D11Device::GetFormatSupportFlags(DXGI_FORMAT Format, UINT* pFlags1, UINT* pFlags2) const {
    const DXGI_VK_FORMAT_INFO fmtMapping = LookupFormat(Format, DXGI_VK_FORMAT_MODE_ANY);

    // Reset output flags preemptively
    if (pFlags1 != nullptr) *pFlags1 = 0;
    if (pFlags2 != nullptr) *pFlags2 = 0;

    // Unsupported or invalid format
    if (Format && fmtMapping.Format == VK_FORMAT_UNDEFINED)
      return E_FAIL;
    
    // Query Vulkan format properties and supported features for it
    const DxvkFormatInfo* fmtProperties = lookupFormatInfo(fmtMapping.Format);

    DxvkFormatFeatures fmtSupport = fmtMapping.Format != VK_FORMAT_UNDEFINED
      ? m_dxvkDevice->getFormatFeatures(fmtMapping.Format)
      : DxvkFormatFeatures();
    
    VkFormatFeatureFlags2 bufFeatures = fmtSupport.buffer;
    VkFormatFeatureFlags2 imgFeatures = fmtSupport.optimal | fmtSupport.linear;

    // For multi-plane images, we want to check available view formats as well
    if (fmtProperties->flags.test(DxvkFormatFlag::MultiPlane)) {
      const VkFormatFeatureFlags2 featureMask
        = VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
        | VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT
        | VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT
        | VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT
        | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      DXGI_VK_FORMAT_FAMILY formatFamily = LookupFamily(Format, DXGI_VK_FORMAT_MODE_ANY);

      for (uint32_t i = 0; i < formatFamily.FormatCount; i++) {
        DxvkFormatFeatures viewFmtSupport = m_dxvkDevice->getFormatFeatures(formatFamily.Formats[i]);
        imgFeatures |= (viewFmtSupport.optimal | viewFmtSupport.linear) & featureMask;
      }
    }
    
    UINT flags1 = 0;
    UINT flags2 = 0;

    // Format can be used for shader resource views with buffers
    if ((bufFeatures & VK_FORMAT_FEATURE_2_UNIFORM_TEXEL_BUFFER_BIT) || !Format)
      flags1 |= D3D11_FORMAT_SUPPORT_BUFFER;
    
    // Format can be used for vertex data
    if (bufFeatures & VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT)
      flags1 |= D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER;
    
    // Format can be used for index data. Only
    // these two formats are supported by D3D11.
    if (Format == DXGI_FORMAT_R16_UINT
     || Format == DXGI_FORMAT_R32_UINT)
      flags1 |= D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER;
    
    // These formats are technically irrelevant since
    // SO buffers are passed in as raw buffers and not
    // as views, but the feature flag exists regardless
    if (Format == DXGI_FORMAT_R32_FLOAT
     || Format == DXGI_FORMAT_R32_UINT
     || Format == DXGI_FORMAT_R32_SINT
     || Format == DXGI_FORMAT_R32G32_FLOAT
     || Format == DXGI_FORMAT_R32G32_UINT
     || Format == DXGI_FORMAT_R32G32_SINT
     || Format == DXGI_FORMAT_R32G32B32_FLOAT
     || Format == DXGI_FORMAT_R32G32B32_UINT
     || Format == DXGI_FORMAT_R32G32B32_SINT
     || Format == DXGI_FORMAT_R32G32B32A32_FLOAT
     || Format == DXGI_FORMAT_R32G32B32A32_UINT
     || Format == DXGI_FORMAT_R32G32B32A32_SINT)
      flags1 |= D3D11_FORMAT_SUPPORT_SO_BUFFER;
    
    if (imgFeatures & (VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT)) {
      const VkFormat depthFormat = LookupFormat(Format, DXGI_VK_FORMAT_MODE_DEPTH).Format;
      
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_1D, 0)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE1D;
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_2D, 0)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE2D;
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_3D, 0)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE3D;

      // We only support tiled resources with a single aspect
      D3D11_TILED_RESOURCES_TIER tiledResourcesTier = m_deviceFeatures.GetTiledResourcesTier();
      VkImageAspectFlags sparseAspects = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

      if (tiledResourcesTier && !(fmtProperties->aspectMask & ~sparseAspects)) {
        VkImageCreateFlags flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                                 | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
                                 | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;

        if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_2D, flags))
          flags2 |= D3D11_FORMAT_SUPPORT2_TILED;
      }

      flags1 |= D3D11_FORMAT_SUPPORT_MIP
             |  D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT;

      // Format can be read 
      if (imgFeatures & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT) {
        flags1 |= D3D11_FORMAT_SUPPORT_TEXTURECUBE
               |  D3D11_FORMAT_SUPPORT_SHADER_LOAD
               |  D3D11_FORMAT_SUPPORT_SHADER_GATHER
               |  D3D11_FORMAT_SUPPORT_SHADER_SAMPLE
               |  D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT;
        
        if (depthFormat != VK_FORMAT_UNDEFINED) {
          flags1 |= D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON
                 |  D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON;
        }
      }
      
      // Format is a color format that can be used for rendering
      if (imgFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT) {
        flags1 |= D3D11_FORMAT_SUPPORT_RENDER_TARGET
               |  D3D11_FORMAT_SUPPORT_MIP_AUTOGEN
               |  D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
        
        if (m_dxvkDevice->features().core.features.logicOp)
          flags2 |= D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP;
      }
      
      // Format supports blending when used for rendering
      if (imgFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT)
        flags1 |= D3D11_FORMAT_SUPPORT_BLENDABLE;
      
      // Format is a depth-stencil format that can be used for rendering
      if (imgFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)
        flags1 |= D3D11_FORMAT_SUPPORT_DEPTH_STENCIL;
      
      // Report supported swap chain formats
      if (Format == DXGI_FORMAT_R8G8B8A8_UNORM
       || Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
       || Format == DXGI_FORMAT_B8G8R8A8_UNORM
       || Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
       || Format == DXGI_FORMAT_R16G16B16A16_FLOAT
       || Format == DXGI_FORMAT_R10G10B10A2_UNORM
       || Format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)
        flags1 |= D3D11_FORMAT_SUPPORT_DISPLAY;
      
      // Query multisample support for this format
      VkImageUsageFlags usage = (fmtProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
        ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

      DxvkFormatQuery formatQuery = { };
      formatQuery.format = fmtMapping.Format;
      formatQuery.type = VK_IMAGE_TYPE_2D;
      formatQuery.tiling = VK_IMAGE_TILING_OPTIMAL;
      formatQuery.usage = usage;

      auto limits = m_dxvkDevice->getFormatLimits(formatQuery);

      if (limits && limits->sampleCounts > VK_SAMPLE_COUNT_1_BIT) {
        flags1 |= D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
               |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE
               |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD;
      }

      // Query whether the format is shareable
      if ((fmtProperties->aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_PLANE_0_BIT))
       && (m_dxvkDevice->features().khrExternalMemoryWin32)) {
        constexpr VkExternalMemoryFeatureFlags featureMask
          = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT
          | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;

        formatQuery.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
        limits = m_dxvkDevice->getFormatLimits(formatQuery);

        if (limits && (limits->externalFeatures & featureMask))
          flags2 |= D3D11_FORMAT_SUPPORT2_SHAREABLE;
      }
    }
    
    // Format can be used for storage images or storage texel buffers
    if ((bufFeatures & VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_BIT)
     && (imgFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT)
     && (imgFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT)) {
      flags1 |= D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW;
      flags2 |= D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
      
      if (m_dxbcOptions.supportsTypedUavLoadR32) {
        // If the R32 formats are supported without format declarations,
        // we can optionally support additional formats for typed loads
        if (imgFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT)
          flags2 |= D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
      } else {
        // Otherwise, we need to emit format declarations, so we can
        // only support the basic set of R32 formats for typed loads
        if (Format == DXGI_FORMAT_R32_FLOAT
         || Format == DXGI_FORMAT_R32_UINT
         || Format == DXGI_FORMAT_R32_SINT)
          flags2 |= D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
      }
      
      if (Format == DXGI_FORMAT_R32_UINT || Format == DXGI_FORMAT_R32_SINT) {
        flags2 |= D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD
               |  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS
               |  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE
               |  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE;
      }
      
      if (Format == DXGI_FORMAT_R32_SINT)
        flags2 |= D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX;
      
      if (Format == DXGI_FORMAT_R32_UINT)
        flags2 |= D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX;
    }

    // Mark everyting as CPU lockable
    if (flags1 | flags2)
      flags1 |= D3D11_FORMAT_SUPPORT_CPU_LOCKABLE;
    
    // Write back format support flags
    if (pFlags1 != nullptr) *pFlags1 = flags1;
    if (pFlags2 != nullptr) *pFlags2 = flags2;
    return (pFlags1 && flags1) || (pFlags2 && flags2) ? S_OK : E_FAIL;
  }
  
  
  BOOL D3D11Device::GetImageTypeSupport(VkFormat Format, VkImageType Type, VkImageCreateFlags Flags) const {
    DxvkFormatQuery formatQuery = { };
    formatQuery.format = Format;
    formatQuery.type = Type;
    formatQuery.tiling = VK_IMAGE_TILING_OPTIMAL;
    formatQuery.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    formatQuery.flags = Flags;

    auto properties = m_dxvkDevice->getFormatLimits(formatQuery);

    if (!properties) {
      formatQuery.tiling = VK_IMAGE_TILING_LINEAR;
      properties = m_dxvkDevice->getFormatLimits(formatQuery);
    }

    return properties.has_value();
  }
  
  
  uint32_t D3D11Device::GetViewPlaneIndex(
          ID3D11Resource*         pResource,
          DXGI_FORMAT             ViewFormat) {
    auto texture = GetCommonTexture(pResource);

    if (!texture)
      return 0;

    uint32_t planeCount = texture->GetPlaneCount();

    if (planeCount == 1)
      return 0;

    auto formatMode   = texture->GetFormatMode();
    auto formatFamily = LookupFamily(texture->Desc()->Format, formatMode);
    auto viewFormat   = LookupFormat(ViewFormat, formatMode);

    for (uint32_t i = 0; i < formatFamily.FormatCount; i++) {
      if (formatFamily.Formats[i] == viewFormat.Format)
        return i % planeCount;
    }

    return ~0u;
  }


  template<bool IsKmtHandle>
  HRESULT D3D11Device::OpenSharedResourceGeneric(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    InitReturnPtr(ppResource);

    if (ppResource == nullptr)
      return S_FALSE;

#ifdef _WIN32
    HANDLE ntHandle = IsKmtHandle ? openKmtHandle(hResource) : hResource;

    if (ntHandle == INVALID_HANDLE_VALUE) {
      Logger::warn(str::format("D3D11Device::OpenSharedResourceGeneric: Handle not found: ", hResource));
      return E_INVALIDARG;
    }

    DxvkSharedTextureMetadata metadata;
    bool ret = getSharedMetadata(ntHandle, &metadata, sizeof(metadata), NULL);

    if (IsKmtHandle)
      ::CloseHandle(ntHandle);

    if (!ret) {
      Logger::warn("D3D11Device::OpenSharedResourceGeneric: Failed to get shared resource info for a texture");
      return E_INVALIDARG;
    }

    D3D11_COMMON_TEXTURE_DESC d3d11Desc;
    d3d11Desc.Width          = metadata.Width;
    d3d11Desc.Height         = metadata.Height;
    d3d11Desc.Depth          = 1,
    d3d11Desc.MipLevels      = metadata.MipLevels;
    d3d11Desc.ArraySize      = metadata.ArraySize;
    d3d11Desc.Format         = metadata.Format;
    d3d11Desc.SampleDesc     = metadata.SampleDesc;
    d3d11Desc.Usage          = metadata.Usage;
    d3d11Desc.BindFlags      = metadata.BindFlags;
    d3d11Desc.CPUAccessFlags = metadata.CPUAccessFlags;
    d3d11Desc.MiscFlags      = metadata.MiscFlags;
    d3d11Desc.TextureLayout  = metadata.TextureLayout;
    if ((d3d11Desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) && !(d3d11Desc.MiscFlags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))) {
      Logger::warn("Fixing up wrong MiscFlags");
      d3d11Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    }

    // Only 2D textures may be shared
    try {
      const Com<D3D11Texture2D> texture = new D3D11Texture2D(this, &d3d11Desc, nullptr, hResource);
      texture->QueryInterface(ReturnedInterface, ppResource);
      return S_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
#else
    Logger::warn("D3D11Device::OpenSharedResourceGeneric: Not supported on this platform.");
    return E_INVALIDARG;
#endif
  }


  template<typename Void>
  void D3D11Device::CopySubresourceData(
          Void*                       pData,
          UINT                        RowPitch,
          UINT                        DepthPitch,
          D3D11CommonTexture*         pTexture,
          UINT                        Subresource,
    const D3D11_BOX*                  pBox) {
    // Validate box against subresource dimensions
    auto formatInfo = lookupFormatInfo(pTexture->GetPackedFormat());
    auto subresource = pTexture->GetSubresourceFromIndex(
      formatInfo->aspectMask, Subresource);

    VkOffset3D offset = { 0, 0, 0 };
    VkExtent3D extent = pTexture->MipLevelExtent(subresource.mipLevel);

    if (pBox) {
      if (pBox->left >= pBox->right
       || pBox->top >= pBox->bottom
       || pBox->front >= pBox->back)
        return;  // legal, but no-op
      
      if (pBox->right > extent.width
       || pBox->bottom > extent.height
       || pBox->back > extent.depth)
        return;  // out of bounds
      
      offset = VkOffset3D {
        int32_t(pBox->left),
        int32_t(pBox->top),
        int32_t(pBox->front) };

      extent = VkExtent3D {
        pBox->right - pBox->left,
        pBox->bottom - pBox->top,
        pBox->back - pBox->front };
    }

    // Copy image data, one plane at a time for multi-plane formats
    Rc<DxvkImage> image = pTexture->GetImage();
    VkDeviceSize dataOffset = 0;

    for (uint32_t i = 0; i < pTexture->GetPlaneCount(); i++) {
      // Find current image aspects to process
      VkImageAspectFlags aspect = formatInfo->aspectMask;

      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane))
        aspect = vk::getPlaneAspect(i);

      // Compute data layout of the current subresource
      D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT layout = pTexture->GetSubresourceLayout(aspect, Subresource);

      // Compute actual map pointer, accounting for the region offset
      VkDeviceSize mapOffset = pTexture->ComputeMappedOffset(Subresource, i, offset);

      void* mapPtr = pTexture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER
        ? pTexture->GetMappedBuffer(Subresource)->mapPtr(mapOffset)
        : image->mapPtr(mapOffset);

      if constexpr (std::is_const<Void>::value) {
        // WriteToSubresource
        auto srcData = reinterpret_cast<const char*>(pData) + dataOffset;

        util::packImageData(mapPtr, srcData, RowPitch, DepthPitch,
          layout.RowPitch, layout.DepthPitch, image->info().type,
          extent, 1, formatInfo, aspect);
      } else {
        // ReadFromSubresource
        auto dstData = reinterpret_cast<char*>(pData) + dataOffset;

        util::packImageData(dstData, mapPtr,
          layout.RowPitch, layout.DepthPitch,
          RowPitch, DepthPitch, image->info().type,
          extent, 1, formatInfo, aspect);
      }

      // Advance linear data pointer by the size of the current aspect
      dataOffset += util::computeImageDataSize(
        pTexture->GetPackedFormat(), extent, aspect);
    }

    // Track dirty texture region if necessary
    if constexpr (std::is_const<Void>::value)
      pTexture->AddDirtyRegion(Subresource, offset, extent);
  }



  D3D11DeviceExt::D3D11DeviceExt(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice)
  : m_container(pContainer), m_device(pDevice) {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11DeviceExt::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11DeviceExt::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeviceExt::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  
  
  BOOL STDMETHODCALLTYPE D3D11DeviceExt::GetExtensionSupport(
          D3D11_VK_EXTENSION      Extension) {
    const auto& deviceFeatures = m_device->GetDXVKDevice()->features();
    
    switch (Extension) {
      case D3D11_VK_EXT_BARRIER_CONTROL:
        return true;
      
      case D3D11_VK_EXT_MULTI_DRAW_INDIRECT:
        return deviceFeatures.core.features.multiDrawIndirect;
        
      case D3D11_VK_EXT_MULTI_DRAW_INDIRECT_COUNT:
        return deviceFeatures.core.features.multiDrawIndirect
            && deviceFeatures.vk12.drawIndirectCount;
      
      case D3D11_VK_EXT_DEPTH_BOUNDS:
        return deviceFeatures.core.features.depthBounds;

      case D3D11_VK_NVX_IMAGE_VIEW_HANDLE:
        return deviceFeatures.nvxImageViewHandle;

      case D3D11_VK_NVX_BINARY_IMPORT:
        return deviceFeatures.nvxBinaryImport
            && deviceFeatures.vk12.bufferDeviceAddress;

      default:
        return false;
    }
  }
  
  
  bool STDMETHODCALLTYPE D3D11DeviceExt::GetCudaTextureObjectNVX(uint32_t srvDriverHandle, uint32_t samplerDriverHandle, uint32_t* pCudaTextureHandle) {
    ID3D11ShaderResourceView* srv = HandleToSrvNVX(srvDriverHandle);

    if (!srv) {
      Logger::warn(str::format("GetCudaTextureObjectNVX() failure - srv handle wasn't found: ", srvDriverHandle));
      return false;
    }

    ID3D11SamplerState* samplerState = HandleToSamplerNVX(samplerDriverHandle);

    if (!samplerState) {
      Logger::warn(str::format("GetCudaTextureObjectNVX() failure - sampler handle wasn't found: ", samplerDriverHandle));
      return false;
    }

    D3D11SamplerState* pSS = static_cast<D3D11SamplerState*>(samplerState);
    Rc<DxvkSampler> pDSS = pSS->GetDXVKSampler();
    VkSampler vkSampler = pDSS->handle();

    D3D11ShaderResourceView* pSRV = static_cast<D3D11ShaderResourceView*>(srv);
    Rc<DxvkImageView> pIV = pSRV->GetImageView();
    VkImageView vkImageView = pIV->handle();

    VkImageViewHandleInfoNVX imageViewHandleInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX};
    imageViewHandleInfo.imageView = vkImageView;
    imageViewHandleInfo.sampler = vkSampler;
    imageViewHandleInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    // note: there's no implicit lifetime management here; it's up to the
    // app to keep the sampler and SRV alive as long as it wants to use this
    // derived handle.
    VkDevice vkDevice = m_device->GetDXVKDevice()->handle();
    *pCudaTextureHandle = m_device->GetDXVKDevice()->vkd()->vkGetImageViewHandleNVX(vkDevice, &imageViewHandleInfo);

    if (!*pCudaTextureHandle) {
      Logger::warn("GetCudaTextureObjectNVX() handle==0 - failed");
      return false;
    }

    return true;
  }
  

  bool STDMETHODCALLTYPE D3D11DeviceExt::CreateCubinComputeShaderWithNameNVX(const void* pCubin, uint32_t size,
      uint32_t blockX, uint32_t blockY, uint32_t blockZ, const char* pShaderName, IUnknown** phShader) {
    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    VkDevice vkDevice = dxvkDevice->handle();

    VkCuModuleCreateInfoNVX moduleCreateInfo = { VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX };
    moduleCreateInfo.pData = pCubin;
    moduleCreateInfo.dataSize = size;

    VkCuModuleNVX cuModule;
    VkCuFunctionNVX cuFunction;
    VkResult result;

    if ((result = dxvkDevice->vkd()->vkCreateCuModuleNVX(vkDevice, &moduleCreateInfo, nullptr, &cuModule))) {
      Logger::warn(str::format("CreateCubinComputeShaderWithNameNVX() - failure to create module - result=", result, " pcubindata=", pCubin, " cubinsize=", size));
      return false; // failure
    }

    VkCuFunctionCreateInfoNVX functionCreateInfo = { VK_STRUCTURE_TYPE_CU_FUNCTION_CREATE_INFO_NVX };
    functionCreateInfo.module = cuModule;
    functionCreateInfo.pName = pShaderName;

    if ((result = dxvkDevice->vkd()->vkCreateCuFunctionNVX(vkDevice, &functionCreateInfo, nullptr, &cuFunction))) {
      dxvkDevice->vkd()->vkDestroyCuModuleNVX(vkDevice, cuModule, nullptr);
      Logger::warn(str::format("CreateCubinComputeShaderWithNameNVX() - failure to create function - result=", result));
      return false;
    }

    *phShader = ref(new CubinShaderWrapper(dxvkDevice,
      cuModule, cuFunction, { blockX, blockY, blockZ }));
    return true;
  }


  bool STDMETHODCALLTYPE D3D11DeviceExt::GetResourceHandleGPUVirtualAddressAndSizeNVX(void* hObject, uint64_t* gpuVAStart, uint64_t* gpuVASize) {
    // The hObject 'opaque driver handle' is really just a straight cast
    // of the corresponding ID3D11Resource* in dxvk/dxvknvapi
    ID3D11Resource* pResource = static_cast<ID3D11Resource*>(hObject);

    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    if (FAILED(GetCommonResourceDesc(pResource, &resourceDesc))) {
      Logger::warn("GetResourceHandleGPUVirtualAddressAndSize() - GetCommonResourceDesc() failed");
      return false;
    }

    switch (resourceDesc.Dim) {
    case D3D11_RESOURCE_DIMENSION_BUFFER:
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
      // okay - we can deal with those two dimensions
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
    default:
      Logger::warn(str::format("GetResourceHandleGPUVirtualAddressAndSize(?) - failure - unsupported dimension: ", resourceDesc.Dim));
      return false;
    }

    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    VkDevice vkDevice = dxvkDevice->handle();

    if (resourceDesc.Dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      D3D11CommonTexture *texture = GetCommonTexture(pResource);
      Rc<DxvkImage> dxvkImage = texture->GetImage();
      if (0 == (dxvkImage->info().usage & (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))) {
        Logger::warn(str::format("GetResourceHandleGPUVirtualAddressAndSize(res=", pResource,") image info missing required usage bit(s); can't be used for vkGetImageViewHandleNVX - failure"));
        return false;
      }

      // The d3d11 nvapi provides us a texture but vulkan only lets us get the GPU address from an imageview.  So, make a private imageview and get the address from that...

      D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;

      const D3D11_COMMON_TEXTURE_DESC *texDesc = texture->Desc();
      if (texDesc->ArraySize != 1) {
        Logger::debug(str::format("GetResourceHandleGPUVirtualAddressAndSize(?) - unexpected array size: ", texDesc->ArraySize));
      }
      resourceViewDesc.Format = texDesc->Format;
      resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      resourceViewDesc.Texture2D.MostDetailedMip = 0;
      resourceViewDesc.Texture2D.MipLevels = texDesc->MipLevels;

      Com<ID3D11ShaderResourceView> pNewSRV;
      HRESULT hr = m_device->CreateShaderResourceView(pResource, &resourceViewDesc, &pNewSRV);
      if (FAILED(hr)) {
        Logger::warn("GetResourceHandleGPUVirtualAddressAndSize() - private CreateShaderResourceView() failed");
        return false;
      }

      Rc<DxvkImageView> dxvkImageView = static_cast<D3D11ShaderResourceView*>(pNewSRV.ptr())->GetImageView();
      VkImageView vkImageView = dxvkImageView->handle();

      VkImageViewAddressPropertiesNVX imageViewAddressProperties = {VK_STRUCTURE_TYPE_IMAGE_VIEW_ADDRESS_PROPERTIES_NVX};

      VkResult res = dxvkDevice->vkd()->vkGetImageViewAddressNVX(vkDevice, vkImageView, &imageViewAddressProperties);
      if (res != VK_SUCCESS) {
        Logger::warn(str::format("GetResourceHandleGPUVirtualAddressAndSize(): vkGetImageViewAddressNVX() result is failure: ", res));
        return false;
      }

      *gpuVAStart = imageViewAddressProperties.deviceAddress;
      *gpuVASize = imageViewAddressProperties.size;
    }
    else if (resourceDesc.Dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      D3D11Buffer *buffer = GetCommonBuffer(pResource);
      const DxvkBufferSliceHandle bufSliceHandle = buffer->GetBuffer()->getSliceHandle();
      VkBuffer vkBuffer = bufSliceHandle.handle;

      VkBufferDeviceAddressInfo bdaInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
      bdaInfo.buffer = vkBuffer;

      VkDeviceAddress bufAddr = dxvkDevice->vkd()->vkGetBufferDeviceAddress(vkDevice, &bdaInfo);
      *gpuVAStart = uint64_t(bufAddr) + bufSliceHandle.offset;
      *gpuVASize = bufSliceHandle.length;
    }

    if (!*gpuVAStart)
        Logger::warn("GetResourceHandleGPUVirtualAddressAndSize() addr==0 - unexpected"); // ... but not explicitly a failure; continue

    return true;
  }


  bool STDMETHODCALLTYPE D3D11DeviceExt::CreateUnorderedAccessViewAndGetDriverHandleNVX(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC*  pDesc, ID3D11UnorderedAccessView** ppUAV, uint32_t* pDriverHandle) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    if (!SUCCEEDED(GetCommonResourceDesc(pResource, &resourceDesc))) {
      Logger::warn("CreateUnorderedAccessViewAndGetDriverHandleNVX() - GetCommonResourceDesc() failed");
      return false;
    }
    if (resourceDesc.Dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::warn(str::format("CreateUnorderedAccessViewAndGetDriverHandleNVX() - failure - unsupported dimension: ", resourceDesc.Dim));
      return false;
    }

    auto texture = GetCommonTexture(pResource);
    Rc<DxvkImage> dxvkImage = texture->GetImage();
    if (0 == (dxvkImage->info().usage & (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))) {
      Logger::warn(str::format("CreateUnorderedAccessViewAndGetDriverHandleNVX(res=", pResource, ") image info missing required usage bit(s); can't be used for vkGetImageViewHandleNVX - failure"));
      return false;
    }

    if (!SUCCEEDED(m_device->CreateUnorderedAccessView(pResource, pDesc, ppUAV))) {
      return false;
    }

    D3D11UnorderedAccessView *pUAV = static_cast<D3D11UnorderedAccessView *>(*ppUAV);
    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    VkDevice vkDevice = dxvkDevice->handle();

    VkImageViewHandleInfoNVX imageViewHandleInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX};
    Rc<DxvkImageView> dxvkImageView = pUAV->GetImageView();
    VkImageView vkImageView = dxvkImageView->handle();

    imageViewHandleInfo.imageView = vkImageView;
    imageViewHandleInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    *pDriverHandle = dxvkDevice->vkd()->vkGetImageViewHandleNVX(vkDevice, &imageViewHandleInfo);

    if (!*pDriverHandle) {
      Logger::warn("CreateUnorderedAccessViewAndGetDriverHandleNVX() handle==0 - failure");
      pUAV->Release();
      return false;
    }

    return true;
  }


  bool STDMETHODCALLTYPE D3D11DeviceExt::CreateShaderResourceViewAndGetDriverHandleNVX(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc, ID3D11ShaderResourceView** ppSRV, uint32_t* pDriverHandle) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    if (!SUCCEEDED(GetCommonResourceDesc(pResource, &resourceDesc))) {
      Logger::warn("CreateShaderResourceViewAndGetDriverHandleNVX() - GetCommonResourceDesc() failed");
      return false;
    }
    if (resourceDesc.Dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::warn(str::format("CreateShaderResourceViewAndGetDriverHandleNVX() - failure - unsupported dimension: ", resourceDesc.Dim));
      return false;
    }

    auto texture = GetCommonTexture(pResource);
    Rc<DxvkImage> dxvkImage = texture->GetImage();
    if (0 == (dxvkImage->info().usage & (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))) {
      Logger::warn(str::format("CreateShaderResourceViewAndGetDriverHandleNVX(res=", pResource, ") image info missing required usage bit(s); can't be used for vkGetImageViewHandleNVX - failure"));
      return false;
    }

    if (!SUCCEEDED(m_device->CreateShaderResourceView(pResource, pDesc, ppSRV))) {
      return false;
    }

    D3D11ShaderResourceView* pSRV = static_cast<D3D11ShaderResourceView*>(*ppSRV);
    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    VkDevice vkDevice = dxvkDevice->handle();

    VkImageViewHandleInfoNVX imageViewHandleInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX};
    Rc<DxvkImageView> dxvkImageView = pSRV->GetImageView();
    VkImageView vkImageView = dxvkImageView->handle();

    imageViewHandleInfo.imageView = vkImageView;
    imageViewHandleInfo.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    *pDriverHandle = dxvkDevice->vkd()->vkGetImageViewHandleNVX(vkDevice, &imageViewHandleInfo);

    if (!*pDriverHandle) {
      Logger::warn("CreateShaderResourceViewAndGetDriverHandleNVX() handle==0 - failure");
      pSRV->Release();
      return false;
    }

    // will need to look-up resource from uint32 handle later
    AddSrvAndHandleNVX(*ppSRV, *pDriverHandle);
    return true;
  }


  bool STDMETHODCALLTYPE D3D11DeviceExt::CreateSamplerStateAndGetDriverHandleNVX(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState, uint32_t* pDriverHandle) {
    if (!SUCCEEDED(m_device->CreateSamplerState(pSamplerDesc, ppSamplerState))) {
      return false;
    }

    // for our purposes the actual value doesn't matter, only its uniqueness
    static std::atomic<ULONG> s_seqNum = 0;
    *pDriverHandle = ++s_seqNum;

    // will need to look-up sampler from uint32 handle later
    AddSamplerAndHandleNVX(*ppSamplerState, *pDriverHandle);
    return true;
  }


  void D3D11DeviceExt::AddSamplerAndHandleNVX(ID3D11SamplerState* pSampler, uint32_t Handle) {
    std::lock_guard lock(m_mapLock);
    m_samplerHandleToPtr[Handle] = pSampler;
  }


  ID3D11SamplerState* D3D11DeviceExt::HandleToSamplerNVX(uint32_t Handle) {
    std::lock_guard lock(m_mapLock);
    auto got = m_samplerHandleToPtr.find(Handle);

    if (got == m_samplerHandleToPtr.end())
      return nullptr;

    return static_cast<ID3D11SamplerState*>(got->second);
  }


  void D3D11DeviceExt::AddSrvAndHandleNVX(ID3D11ShaderResourceView* pSrv, uint32_t Handle) {
    std::lock_guard lock(m_mapLock);
    m_srvHandleToPtr[Handle] = pSrv;
  }


  ID3D11ShaderResourceView* D3D11DeviceExt::HandleToSrvNVX(uint32_t Handle) {
    std::lock_guard lock(m_mapLock);
    auto got = m_srvHandleToPtr.find(Handle);

    if (got == m_srvHandleToPtr.end())
      return nullptr;

    return static_cast<ID3D11ShaderResourceView*>(got->second);
  }



  
  
  D3D11VideoDevice::D3D11VideoDevice(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice)
  : m_container(pContainer), m_device(pDevice) {

  }


  D3D11VideoDevice::~D3D11VideoDevice() {

  }


  ULONG STDMETHODCALLTYPE D3D11VideoDevice::AddRef() {
    return m_container->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D11VideoDevice::Release() {
    return m_container->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoDecoder(
    const D3D11_VIDEO_DECODER_DESC*                     pVideoDesc,
    const D3D11_VIDEO_DECODER_CONFIG*                   pConfig,
          ID3D11VideoDecoder**                          ppDecoder) {
    Logger::err("D3D11VideoDevice::CreateVideoDecoder: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoProcessor(
          ID3D11VideoProcessorEnumerator*               pEnum,
          UINT                                          RateConversionIndex,
          ID3D11VideoProcessor**                        ppVideoProcessor) {
    try {
      auto enumerator = static_cast<D3D11VideoProcessorEnumerator*>(pEnum);
      *ppVideoProcessor = ref(new D3D11VideoProcessor(m_device, enumerator, RateConversionIndex));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateAuthenticatedChannel(
          D3D11_AUTHENTICATED_CHANNEL_TYPE              ChannelType,
          ID3D11AuthenticatedChannel**                  ppAuthenticatedChannel) {
    Logger::err("D3D11VideoDevice::CreateAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateCryptoSession(
    const GUID*                                         pCryptoType,
    const GUID*                                         pDecoderProfile,
    const GUID*                                         pKeyExchangeType,
          ID3D11CryptoSession**                         ppCryptoSession) {
    Logger::err("D3D11VideoDevice::CreateCryptoSession: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoDecoderOutputView(
          ID3D11Resource*                               pResource,
    const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*         pDesc,
          ID3D11VideoDecoderOutputView**                ppVDOVView) {
    Logger::err("D3D11VideoDevice::CreateVideoDecoderOutputView: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoProcessorInputView(
          ID3D11Resource*                               pResource,
          ID3D11VideoProcessorEnumerator*               pEnum,
    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC*        pDesc,
          ID3D11VideoProcessorInputView**               ppVPIView) {
    try {
      *ppVPIView = ref(new D3D11VideoProcessorInputView(m_device, pResource, *pDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoProcessorOutputView(
          ID3D11Resource*                               pResource,
          ID3D11VideoProcessorEnumerator*               pEnum,
    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC*       pDesc,
          ID3D11VideoProcessorOutputView**              ppVPOView) {
    try {
      *ppVPOView = ref(new D3D11VideoProcessorOutputView(m_device, pResource, *pDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CreateVideoProcessorEnumerator(
    const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*           pDesc,
          ID3D11VideoProcessorEnumerator**              ppEnum)  {
    try {
      *ppEnum = ref(new D3D11VideoProcessorEnumerator(m_device, *pDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }


  UINT STDMETHODCALLTYPE D3D11VideoDevice::GetVideoDecoderProfileCount() {
    Logger::err("D3D11VideoDevice::GetVideoDecoderProfileCount: Stub");
    return 0;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::GetVideoDecoderProfile(
          UINT                                          Index,
          GUID*                                         pDecoderProfile) {
    Logger::err("D3D11VideoDevice::GetVideoDecoderProfile: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CheckVideoDecoderFormat(
    const GUID*                                         pDecoderProfile,
          DXGI_FORMAT                                   Format,
          BOOL*                                         pSupported) {
    Logger::err("D3D11VideoDevice::CheckVideoDecoderFormat: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::GetVideoDecoderConfigCount(
    const D3D11_VIDEO_DECODER_DESC*                     pDesc,
          UINT*                                         pCount) {
    Logger::err("D3D11VideoDevice::GetVideoDecoderConfigCount: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::GetVideoDecoderConfig(
    const D3D11_VIDEO_DECODER_DESC*                     pDesc,
          UINT                                          Index,
          D3D11_VIDEO_DECODER_CONFIG*                   pConfig) {
    Logger::err("D3D11VideoDevice::GetVideoDecoderConfig: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::GetContentProtectionCaps(
    const GUID*                                         pCryptoType,
    const GUID*                                         pDecoderProfile,
          D3D11_VIDEO_CONTENT_PROTECTION_CAPS*          pCaps) {
    Logger::err("D3D11VideoDevice::GetContentProtectionCaps: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::CheckCryptoKeyExchange(
    const GUID*                                         pCryptoType,
    const GUID*                                         pDecoderProfile,
          UINT                                          Index,
          GUID*                                         pKeyExchangeType) {
    Logger::err("D3D11VideoDevice::CheckCryptoKeyExchange: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::SetPrivateData(
          REFGUID                                       Name,
          UINT                                          DataSize,
    const void*                                         pData) {
    return m_container->SetPrivateData(Name, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDevice::SetPrivateDataInterface(
          REFGUID                                       Name,
    const IUnknown*                                     pData) {
    return m_container->SetPrivateDataInterface(Name, pData);
  }




  DXGIVkSwapChainFactory::DXGIVkSwapChainFactory(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice)
  : m_container(pContainer), m_device(pDevice) {
    
  }


  ULONG STDMETHODCALLTYPE DXGIVkSwapChainFactory::AddRef() {
    return m_device->AddRef();
  }


  ULONG STDMETHODCALLTYPE DXGIVkSwapChainFactory::Release() {
    return m_device->Release();
  }


  HRESULT STDMETHODCALLTYPE DXGIVkSwapChainFactory::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE DXGIVkSwapChainFactory::CreateSwapChain(
          IDXGIVkSurfaceFactory*    pSurfaceFactory,
    const DXGI_SWAP_CHAIN_DESC1*    pDesc,
          IDXGIVkSwapChain**        ppSwapChain) {
    InitReturnPtr(ppSwapChain);

    try {
      auto vki = m_device->GetDXVKDevice()->adapter()->vki();

      Com<D3D11SwapChain> presenter = new D3D11SwapChain(
        m_container, m_device, pSurfaceFactory, pDesc);
      
      *ppSwapChain = presenter.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }




  DXGIDXVKDevice::DXGIDXVKDevice(D3D11DXGIDevice* pContainer)
  : m_container(pContainer), m_apiVersion(11) {

  }
  

  ULONG STDMETHODCALLTYPE DXGIDXVKDevice::AddRef() {
    return m_container->AddRef();
  }
  

  ULONG STDMETHODCALLTYPE DXGIDXVKDevice::Release() {
    return m_container->Release();
  }
  

  HRESULT STDMETHODCALLTYPE DXGIDXVKDevice::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }


  void STDMETHODCALLTYPE DXGIDXVKDevice::SetAPIVersion(
            UINT                    Version) {
    m_apiVersion = Version;
  }


  UINT STDMETHODCALLTYPE DXGIDXVKDevice::GetAPIVersion() {
    return m_apiVersion;
  }

  


  D3D11DXGIDevice::D3D11DXGIDevice(
          IDXGIAdapter*       pAdapter,
          ID3D12Device*       pD3D12Device,
          ID3D12CommandQueue* pD3D12Queue,
          Rc<DxvkInstance>    pDxvkInstance,
          Rc<DxvkAdapter>     pDxvkAdapter,
          Rc<DxvkDevice>      pDxvkDevice,
          D3D_FEATURE_LEVEL   FeatureLevel,
          UINT                FeatureFlags)
  : m_dxgiAdapter   (pAdapter),
    m_dxvkInstance  (pDxvkInstance),
    m_dxvkAdapter   (pDxvkAdapter),
    m_dxvkDevice    (pDxvkDevice),
    m_d3d11Device   (this, FeatureLevel, FeatureFlags),
    m_d3d11DeviceExt(this, &m_d3d11Device),
    m_d3d11Interop  (this, &m_d3d11Device),
    m_d3d11Video    (this, &m_d3d11Device),
    m_d3d11on12     (this, &m_d3d11Device, pD3D12Device, pD3D12Queue),
    m_metaDevice    (this),
    m_dxvkFactory   (this, &m_d3d11Device) {

  }
  
  
  D3D11DXGIDevice::~D3D11DXGIDevice() {

  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDevice)
     || riid == __uuidof(IDXGIDevice1)
     || riid == __uuidof(IDXGIDevice2)
     || riid == __uuidof(IDXGIDevice3)
     || riid == __uuidof(IDXGIDevice4)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIVkInteropDevice)
     || riid == __uuidof(IDXGIVkInteropDevice1)) {
      *ppvObject = ref(&m_d3d11Interop);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10Device)
     || riid == __uuidof(ID3D10Device1)) {
      *ppvObject = ref(m_d3d11Device.GetD3D10Interface());
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D11Device)
     || riid == __uuidof(ID3D11Device1)
     || riid == __uuidof(ID3D11Device2)
     || riid == __uuidof(ID3D11Device3)
     || riid == __uuidof(ID3D11Device4)
     || riid == __uuidof(ID3D11Device5)) {
      *ppvObject = ref(&m_d3d11Device);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D11VkExtDevice)
     || riid == __uuidof(ID3D11VkExtDevice1)) {
      *ppvObject = ref(&m_d3d11DeviceExt);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIDXVKDevice)) {
      *ppvObject = ref(&m_metaDevice);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIVkSwapChainFactory)) {
      *ppvObject = ref(&m_dxvkFactory);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11VideoDevice)) {
      *ppvObject = ref(&m_d3d11Video);
      return S_OK;
    }

    if (m_d3d11on12.Is11on12Device()) {
      if (riid == __uuidof(ID3D11On12Device)) {
        *ppvObject = ref(&m_d3d11on12);
        return S_OK;
      }
    }

    if (riid == __uuidof(ID3D10Multithread)) {
      Com<ID3D11DeviceContext> context;
      m_d3d11Device.GetImmediateContext(&context);
      return context->QueryInterface(riid, ppvObject);
    }

    if (riid == __uuidof(ID3D11Debug))
      return E_NOINTERFACE;      
    
    // Undocumented interfaces that are queried by some games
    if (riid == GUID{0xd56e2a4c,0x5127,0x8437,{0x65,0x8a,0x98,0xc5,0xbb,0x78,0x94,0x98}})
      return E_NOINTERFACE;
    
    if (logQueryInterfaceError(__uuidof(IDXGIDXVKDevice), riid)) {
      Logger::warn("D3D11DXGIDevice::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::GetParent(
          REFIID                  riid,
          void**                  ppParent) {
    return m_dxgiAdapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::CreateSurface(
    const DXGI_SURFACE_DESC*    pDesc,
          UINT                  NumSurfaces,
          DXGI_USAGE            Usage,
    const DXGI_SHARED_RESOURCE* pSharedResource,
          IDXGISurface**        ppSurface) {
    if (!pDesc || (NumSurfaces && !ppSurface))
      return E_INVALIDARG;
    
    D3D11_TEXTURE2D_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.MipLevels      = 1;
    desc.ArraySize      = 1;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = pDesc->SampleDesc;
    desc.BindFlags      = 0;
    desc.MiscFlags      = 0;

    // Handle bind flags
    if (Usage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    if (Usage & DXGI_USAGE_SHADER_INPUT)
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    if (Usage & DXGI_USAGE_UNORDERED_ACCESS)
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    // Handle CPU access flags
    switch (Usage & DXGI_CPU_ACCESS_FIELD) {
      case DXGI_CPU_ACCESS_NONE:
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        break;

      case DXGI_CPU_ACCESS_DYNAMIC:
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        break;

      case DXGI_CPU_ACCESS_READ_WRITE:
      case DXGI_CPU_ACCESS_SCRATCH:
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        break;

      default:
        return E_INVALIDARG;
    }

    // Restrictions and limitations of CreateSurface are not
    // well-documented, so we'll be a lenient on validation.
    HRESULT hr = m_d3d11Device.CreateTexture2D(&desc, nullptr, nullptr);

    if (FAILED(hr))
      return hr;

    // We don't support shared resources
    if (NumSurfaces && pSharedResource)
      Logger::err("D3D11: CreateSurface: Shared surfaces not supported");

    // Try to create the given number of surfaces
    uint32_t surfacesCreated = 0;
    hr = S_OK;

    for (uint32_t i = 0; i < NumSurfaces; i++) {
      Com<ID3D11Texture2D> texture;

      hr = m_d3d11Device.CreateTexture2D(&desc, nullptr, &texture);

      if (SUCCEEDED(hr)) {
        hr = texture->QueryInterface(__uuidof(IDXGISurface),
          reinterpret_cast<void**>(&ppSurface[i]));
        surfacesCreated = i + 1;
      }

      if (FAILED(hr))
        break;
    }

    // Don't leak surfaces if we failed to create one
    if (FAILED(hr)) {
      for (uint32_t i = 0; i < surfacesCreated; i++)
        ppSurface[i]->Release();
    }

    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::GetAdapter(
          IDXGIAdapter**        pAdapter) {
    if (pAdapter == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pAdapter = m_dxgiAdapter.ref();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::GetGPUThreadPriority(
          INT*                  pPriority) {
    *pPriority = 0;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::QueryResourceResidency(
          IUnknown* const*      ppResources,
          DXGI_RESIDENCY*       pResidencyStatus,
          UINT                  NumResources) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DXGIDevice::QueryResourceResidency: Stub");
    
    if (!ppResources || !pResidencyStatus)
      return E_INVALIDARG;

    for (uint32_t i = 0; i < NumResources; i++)
      pResidencyStatus[i] = DXGI_RESIDENCY_FULLY_RESIDENT;

    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::SetGPUThreadPriority(
          INT                   Priority) {
    if (Priority < -7 || Priority > 7)
      return E_INVALIDARG;
    
    Logger::err("DXGI: SetGPUThreadPriority: Ignoring");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::GetMaximumFrameLatency(
          UINT*                 pMaxLatency) {
    if (!pMaxLatency)
      return DXGI_ERROR_INVALID_CALL;
    
    *pMaxLatency = m_frameLatency;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::SetMaximumFrameLatency(
          UINT                  MaxLatency) {
    if (MaxLatency == 0)
      MaxLatency = DefaultFrameLatency;
    
    if (MaxLatency > DXGI_MAX_SWAP_CHAIN_BUFFERS)
      return DXGI_ERROR_INVALID_CALL;
    
    m_frameLatency = MaxLatency;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::OfferResources( 
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          DXGI_OFFER_RESOURCE_PRIORITY  Priority) {
    return OfferResources1(NumResources, ppResources, Priority, 0);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::OfferResources1( 
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          DXGI_OFFER_RESOURCE_PRIORITY  Priority,
          UINT                          Flags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11DXGIDevice::OfferResources1: Stub");

    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::ReclaimResources( 
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          BOOL*                         pDiscarded) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11DXGIDevice::ReclaimResources: Stub");

    if (pDiscarded)
      *pDiscarded = false;

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::ReclaimResources1(
          UINT                          NumResources,
          IDXGIResource* const*         ppResources,
          DXGI_RECLAIM_RESOURCE_RESULTS* pResults) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11DXGIDevice::ReclaimResources1: Stub");

    if (pResults) {
      for (uint32_t i = 0; i < NumResources; i++)
        pResults[i] = DXGI_RECLAIM_RESOURCE_RESULT_OK;
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIDevice::EnqueueSetEvent(HANDLE hEvent) {
    auto immediateContext = m_d3d11Device.GetContext();
    immediateContext->Flush1(D3D11_CONTEXT_TYPE_ALL, hEvent);
    return S_OK;            
  }


  void STDMETHODCALLTYPE D3D11DXGIDevice::Trim() {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11DXGIDevice::Trim: Stub");
  }
  
  
  Rc<DxvkDevice> STDMETHODCALLTYPE D3D11DXGIDevice::GetDXVKDevice() {
    return m_dxvkDevice;
  }

}
