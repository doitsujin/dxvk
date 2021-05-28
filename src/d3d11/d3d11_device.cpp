#include <algorithm>
#include <cstring>

#include "../dxgi/dxgi_monitor.h"
#include "../dxgi/dxgi_swapchain.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_instance.h"

#include "d3d11_buffer.h"
#include "d3d11_class_linkage.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
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

namespace dxvk {
  
  constexpr uint32_t D3D11DXGIDevice::DefaultFrameLatency;



  D3D11Device::D3D11Device(
          D3D11DXGIDevice*    pContainer,
          D3D_FEATURE_LEVEL   FeatureLevel,
          UINT                FeatureFlags)
  : m_container     (pContainer),
    m_featureLevel  (FeatureLevel),
    m_featureFlags  (FeatureFlags),
    m_dxvkDevice    (pContainer->GetDXVKDevice()),
    m_dxvkAdapter   (m_dxvkDevice->adapter()),
    m_d3d11Formats  (m_dxvkAdapter),
    m_d3d11Options  (m_dxvkDevice->instance()->config(), m_dxvkDevice),
    m_dxbcOptions   (m_dxvkDevice, m_d3d11Options) {
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

    if (!ppBuffer)
      return S_FALSE;
    
    try {
      const Com<D3D11Buffer> buffer = new D3D11Buffer(this, &desc);
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
    
    if (!ppTexture1D)
      return S_FALSE;
    
    try {
      const Com<D3D11Texture1D> texture = new D3D11Texture1D(this, &desc);
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

    if (FAILED(hr))
      return hr;
    
    if (!ppTexture2D)
      return S_FALSE;
    
    try {
      Com<D3D11Texture2D> texture = new D3D11Texture2D(this, &desc);
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
    
    if (!ppTexture3D)
      return S_FALSE;
      
    try {
      Com<D3D11Texture3D> texture = new D3D11Texture3D(this, &desc);
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

    if (pInputElementDescs == nullptr)
      return E_INVALIDARG;
    
    try {
      DxbcReader dxbcReader(reinterpret_cast<const char*>(
        pShaderBytecodeWithInputSignature), BytecodeLength);
      DxbcModule dxbcModule(dxbcReader);
      
      const Rc<DxbcIsgn> inputSignature = dxbcModule.isgn();

      uint32_t attrMask = 0;
      uint32_t bindMask = 0;
      
      std::array<DxvkVertexAttribute, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> attrList;
      std::array<DxvkVertexBinding,   D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> bindList;
      
      for (uint32_t i = 0; i < NumElements; i++) {
        const DxbcSgnEntry* entry = inputSignature->find(
          pInputElementDescs[i].SemanticName,
          pInputElementDescs[i].SemanticIndex, 0);
        
        if (entry == nullptr) {
          Logger::debug(str::format(
            "D3D11Device: No such vertex shader semantic: ",
            pInputElementDescs[i].SemanticName,
            pInputElementDescs[i].SemanticIndex));
        }
        
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
        const DxvkFormatInfo* formatInfo = imageFormatInfo(attrib.format);
        VkDeviceSize alignment = std::min<VkDeviceSize>(formatInfo->elementSize, 4);

        if (attrib.offset == D3D11_APPEND_ALIGNED_ELEMENT) {
          attrib.offset = 0;
          
          for (uint32_t j = 1; j <= i; j++) {
            const DxvkVertexAttribute& prev = attrList.at(i - j);
            
            if (prev.binding == attrib.binding) {
              attrib.offset = align(prev.offset + imageFormatInfo(prev.format)->elementSize, alignment);
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
        
        // Check if the binding was already defined. If so, the
        // parameters must be identical (namely, the input rate).
        bool bindingDefined = false;
        
        for (uint32_t j = 0; j < i; j++) {
          uint32_t bindingId = attrList.at(j).binding;

          if (binding.binding == bindingId) {
            bindingDefined = true;
            
            if (binding.inputRate != bindList.at(bindingId).inputRate) {
              Logger::err(str::format(
                "D3D11Device: Conflicting input rate for binding ",
                binding.binding));
              return E_INVALIDARG;
            }
          }
        }

        if (!bindingDefined)
          bindList.at(binding.binding) = binding;
        
        if (entry != nullptr) {
          attrMask |= 1u << i;
          bindMask |= 1u << binding.binding;
        }
      }

      // Compact the attribute and binding lists to filter
      // out attributes and bindings not used by the shader
      uint32_t attrCount = CompactSparseList(attrList.data(), attrMask);
      uint32_t bindCount = CompactSparseList(bindList.data(), bindMask);

      // Check if there are any semantics defined in the
      // shader that are not included in the current input
      // layout.
      for (auto i = inputSignature->begin(); i != inputSignature->end(); i++) {
        bool found = i->systemValue != DxbcSystemValue::None;
        
        for (uint32_t j = 0; j < attrCount && !found; j++)
          found = attrList.at(j).location == i->registerId;
        
        if (!found) {
          Logger::warn(str::format(
            "D3D11Device: Vertex input '",
            i->semanticName, i->semanticIndex,
            "' not defined by input layout"));
        }
      }
      
      // Create the actual input layout object
      // if the application requests it.
      if (ppInputLayout != nullptr) {
        *ppInputLayout = ref(
          new D3D11InputLayout(this,
            attrCount, attrList.data(),
            bindCount, bindList.data()));
      }
      
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
     && !m_dxvkDevice->extensions().extConservativeRasterization)
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
    
    if (ppSamplerState == nullptr)
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

    if (!pFeatureLevels || FeatureLevels == 0)
      return E_INVALIDARG;
    
    if (EmulatedInterface != __uuidof(ID3D10Device)
     && EmulatedInterface != __uuidof(ID3D10Device1)
     && EmulatedInterface != __uuidof(ID3D11Device)
     && EmulatedInterface != __uuidof(ID3D11Device1))
      return E_INVALIDARG;
    
    UINT flId;
    for (flId = 0; flId < FeatureLevels; flId++) {
      if (CheckFeatureLevelSupport(m_dxvkDevice->instance(), m_dxvkAdapter, pFeatureLevels[flId]))
        break;
    }

    if (flId == FeatureLevels)
      return E_INVALIDARG;

    if (pFeatureLevels[flId] > m_featureLevel)
      m_featureLevel = pFeatureLevels[flId];
    
    if (pChosenFeatureLevel)
      *pChosenFeatureLevel = pFeatureLevels[flId];
    
    if (!ppContextState)
      return S_FALSE;
    
    *ppContextState = ref(new D3D11DeviceContextState(this));
    return S_OK;
  }
  

  HRESULT STDMETHODCALLTYPE D3D11Device::CreateFence(
          UINT64                      InitialValue,
          D3D11_FENCE_FLAG            Flags,
          REFIID                      ReturnedInterface,
          void**                      ppFence) {
    InitReturnPtr(ppFence);

    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11Device::CreateFence: Not implemented");
    
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11Device::ReadFromSubresource(
          void*                       pDstData,
          UINT                        DstRowPitch,
          UINT                        DstDepthPitch,
          ID3D11Resource*             pSrcResource,
          UINT                        SrcSubresource,
    const D3D11_BOX*                  pSrcBox) {
    CopySubresourceData(
      pDstData, DstRowPitch, DstDepthPitch,
      pSrcResource, SrcSubresource, pSrcBox);
  }


  void STDMETHODCALLTYPE D3D11Device::WriteToSubresource(
          ID3D11Resource*             pDstResource,
          UINT                        DstSubresource,
    const D3D11_BOX*                  pDstBox,
    const void*                       pSrcData,
          UINT                        SrcRowPitch,
          UINT                        SrcDepthPitch) {
    CopySubresourceData(
      pSrcData, SrcRowPitch, SrcRowPitch,
      pDstResource, DstSubresource, pDstBox);
  }


  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    InitReturnPtr(ppResource);
    
    Logger::err("D3D11Device::OpenSharedResource: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource1(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    InitReturnPtr(ppResource);
    
    Logger::err("D3D11Device::OpenSharedResource1: Not implemented");
    return E_NOTIMPL;
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

    Logger::err("D3D11Device::OpenSharedFence: Not implemented");
    return E_NOTIMPL;
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
    
    // Check if the device supports the given combination of format
    // and sample count. D3D exposes the opaque concept of quality
    // levels to the application, we'll just define one such level.
    VkImageFormatProperties formatProps;
    
    VkResult status = m_dxvkAdapter->imageFormatProperties(
      format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT, 0, formatProps);
    
    if ((status == VK_SUCCESS) && (formatProps.sampleCounts & sampleCountFlag))
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
      case D3D11_FEATURE_THREADING: {
        auto info = static_cast<D3D11_FEATURE_DATA_THREADING*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        // We report native support for command lists here so that we do not actually
        // have to re-implement the UpdateSubresource bug from the D3D11 runtime, see
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476486(v=vs.85).aspx)
        info->DriverConcurrentCreates = TRUE;
        info->DriverCommandLists      = TRUE;
      } return S_OK;
      
      case D3D11_FEATURE_DOUBLES: {
        auto info = static_cast<D3D11_FEATURE_DATA_DOUBLES*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        info->DoublePrecisionFloatShaderOps = m_dxvkDevice->features().core.features.shaderFloat64
                                           && m_dxvkDevice->features().core.features.shaderInt64;
      } return S_OK;
      
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
      
      case D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        info->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = TRUE;
      } return S_OK;
      
      case D3D11_FEATURE_D3D11_OPTIONS: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D11_OPTIONS*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        // https://msdn.microsoft.com/en-us/library/windows/desktop/hh404457(v=vs.85).aspx
        const auto& features = m_dxvkDevice->features();

        info->OutputMergerLogicOp                     = features.core.features.logicOp;
        info->UAVOnlyRenderingForcedSampleCount       = features.core.features.variableMultisampleRate;
        info->DiscardAPIsSeenByDriver                 = TRUE;
        info->FlagsForUpdateAndCopySeenByDriver       = TRUE;
        info->ClearView                               = TRUE;
        info->CopyWithOverlap                         = TRUE;
        info->ConstantBufferPartialUpdate             = TRUE;
        info->ConstantBufferOffsetting                = TRUE;
        info->MapNoOverwriteOnDynamicConstantBuffer   = TRUE;
        info->MapNoOverwriteOnDynamicBufferSRV        = TRUE;
        info->MultisampleRTVWithForcedSampleCountOne  = TRUE; /* not really */
        info->SAD4ShaderInstructions                  = TRUE;
        info->ExtendedDoublesShaderInstructions       = TRUE;
        info->ExtendedResourceSharing                 = TRUE; /* not really */
      } return S_OK;

      case D3D11_FEATURE_ARCHITECTURE_INFO: {
        auto info = static_cast<D3D11_FEATURE_DATA_ARCHITECTURE_INFO*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->TileBasedDeferredRenderer = FALSE;
      } return S_OK;

      case D3D11_FEATURE_D3D9_OPTIONS: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D9_OPTIONS*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->FullNonPow2TextureSupport = TRUE;
      } return S_OK;
      
      case D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        // Report that we only support full 32-bit operations
        info->PixelShaderMinPrecision          = 0;
        info->AllOtherShaderStagesMinPrecision = 0;
      } return S_OK;
      
      case D3D11_FEATURE_D3D9_SHADOW_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;
        
        info->SupportsDepthAsTextureWithLessEqualComparisonFilter = TRUE;
      } return S_OK;

      case D3D11_FEATURE_D3D11_OPTIONS1: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D11_OPTIONS1*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        // Min/Max filtering requires Tiled Resources Tier 2 for some reason,
        // so we cannot support it even though Vulkan exposes this feature
        info->TiledResourcesTier                    = D3D11_TILED_RESOURCES_NOT_SUPPORTED;
        info->MinMaxFiltering                       = FALSE;
        info->ClearViewAlsoSupportsDepthOnlyFormats = TRUE;
        info->MapOnDefaultBuffers                   = TRUE;
      } return S_OK;

      case D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->SimpleInstancingSupported = TRUE;
      } return S_OK;

      case D3D11_FEATURE_MARKER_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_MARKER_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->Profile = FALSE;
      } return S_OK;

      case D3D11_FEATURE_D3D9_OPTIONS1: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D9_OPTIONS1*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->FullNonPow2TextureSupported                                 = TRUE;
        info->DepthAsTextureWithLessEqualComparisonFilterSupported        = TRUE;
        info->SimpleInstancingSupported                                   = TRUE;
        info->TextureCubeFaceRenderTargetWithNonCubeDepthStencilSupported = TRUE;
      } return S_OK;

      case D3D11_FEATURE_D3D11_OPTIONS2: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D11_OPTIONS2*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        const auto& extensions = m_dxvkDevice->extensions();
        const auto& features = m_dxvkDevice->features();

        info->PSSpecifiedStencilRefSupported = extensions.extShaderStencilExport;
        info->TypedUAVLoadAdditionalFormats  = features.core.features.shaderStorageImageReadWithoutFormat;
        info->ROVsSupported                  = FALSE;
        info->ConservativeRasterizationTier  = D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED;
        info->MapOnDefaultTextures           = TRUE;
        info->TiledResourcesTier             = D3D11_TILED_RESOURCES_NOT_SUPPORTED;
        info->StandardSwizzle                = FALSE;
        info->UnifiedMemoryArchitecture      = m_dxvkDevice->isUnifiedMemoryArchitecture();

        if (m_dxvkDevice->extensions().extConservativeRasterization) {
          // We don't have a way to query uncertainty regions, so just check degenerate triangle behaviour
          info->ConservativeRasterizationTier = m_dxvkDevice->properties().extConservativeRasterization.degenerateTrianglesRasterized
            ? D3D11_CONSERVATIVE_RASTERIZATION_TIER_2 : D3D11_CONSERVATIVE_RASTERIZATION_TIER_1;
        }
      } return S_OK;

      case D3D11_FEATURE_D3D11_OPTIONS3: {
        if (FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS3))
          return E_INVALIDARG;

        const auto& extensions = m_dxvkDevice->extensions();

        auto info = static_cast<D3D11_FEATURE_DATA_D3D11_OPTIONS3*>(pFeatureSupportData);
        info->VPAndRTArrayIndexFromAnyShaderFeedingRasterizer = extensions.extShaderViewportIndexLayer;
      } return S_OK;

      case D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT: {
        auto info = static_cast<D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        // These numbers are not accurate, but it should not have any effect on D3D11 apps
        info->MaxGPUVirtualAddressBitsPerResource = 32;
        info->MaxGPUVirtualAddressBitsPerProcess  = 40;
      } return S_OK;

      case D3D11_FEATURE_D3D11_OPTIONS4: {
        auto info = static_cast<D3D11_FEATURE_DATA_D3D11_OPTIONS4*>(pFeatureSupportData);

        if (FeatureSupportDataSize != sizeof(*info))
          return E_INVALIDARG;

        info->ExtendedNV12SharedTextureSupported = FALSE;
      } return S_OK;

      default:
        Logger::err(str::format("D3D11Device: CheckFeatureSupport: Unknown feature: ", Feature));
        return E_INVALIDARG;
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
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11Device::GetResourceTiling: Tiled resources not supported");

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
  
  
  void D3D11Device::FlushInitContext() {
    m_initializer->Flush();
  }
  
  
  bool D3D11Device::CheckFeatureLevelSupport(
    const Rc<DxvkInstance>& instance,
    const Rc<DxvkAdapter>&  adapter,
          D3D_FEATURE_LEVEL featureLevel) {
    if (featureLevel > GetMaxFeatureLevel(instance))
      return false;
    
    // Check whether all features are supported
    const DxvkDeviceFeatures features
      = GetDeviceFeatures(adapter, featureLevel);
    
    if (!adapter->checkFeatureSupport(features))
      return false;
    
    // TODO also check for required limits
    return true;
  }
  
  
  DxvkDeviceFeatures D3D11Device::GetDeviceFeatures(
    const Rc<DxvkAdapter>&  adapter,
          D3D_FEATURE_LEVEL featureLevel) {
    DxvkDeviceFeatures supported = adapter->features();
    DxvkDeviceFeatures enabled   = {};

    enabled.core.features.geometryShader                          = VK_TRUE;
    enabled.core.features.robustBufferAccess                      = VK_TRUE;
    enabled.core.features.shaderStorageImageWriteWithoutFormat    = VK_TRUE;
    enabled.core.features.depthBounds                             = supported.core.features.depthBounds;

    enabled.shaderDrawParameters.shaderDrawParameters             = VK_TRUE;

    enabled.extMemoryPriority.memoryPriority                      = supported.extMemoryPriority.memoryPriority;

    enabled.extRobustness2.robustBufferAccess2                    = supported.extRobustness2.robustBufferAccess2;
    enabled.extRobustness2.robustImageAccess2                     = supported.extRobustness2.robustImageAccess2;
    enabled.extRobustness2.nullDescriptor                         = supported.extRobustness2.nullDescriptor;

    enabled.extShaderDemoteToHelperInvocation.shaderDemoteToHelperInvocation  = supported.extShaderDemoteToHelperInvocation.shaderDemoteToHelperInvocation;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor      = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor  = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;
    
    if (supported.extCustomBorderColor.customBorderColorWithoutFormat) {
      enabled.extCustomBorderColor.customBorderColors             = VK_TRUE;
      enabled.extCustomBorderColor.customBorderColorWithoutFormat = VK_TRUE;
    }

    if (featureLevel >= D3D_FEATURE_LEVEL_9_1) {
      enabled.core.features.depthClamp                            = VK_TRUE;
      enabled.core.features.depthBiasClamp                        = VK_TRUE;
      enabled.core.features.fillModeNonSolid                      = VK_TRUE;
      enabled.core.features.pipelineStatisticsQuery               = supported.core.features.pipelineStatisticsQuery;
      enabled.core.features.sampleRateShading                     = VK_TRUE;
      enabled.core.features.samplerAnisotropy                     = supported.core.features.samplerAnisotropy;
      enabled.core.features.shaderClipDistance                    = VK_TRUE;
      enabled.core.features.shaderCullDistance                    = VK_TRUE;
      enabled.core.features.textureCompressionBC                  = VK_TRUE;
      enabled.extDepthClipEnable.depthClipEnable                  = supported.extDepthClipEnable.depthClipEnable;
      enabled.extHostQueryReset.hostQueryReset                    = supported.extHostQueryReset.hostQueryReset;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_2) {
      enabled.core.features.occlusionQueryPrecise                 = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3) {
      enabled.core.features.independentBlend                      = VK_TRUE;
      enabled.core.features.multiViewport                         = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) {
      enabled.core.features.fullDrawIndexUint32                   = VK_TRUE;
      enabled.core.features.logicOp                               = supported.core.features.logicOp;
      enabled.core.features.shaderImageGatherExtended             = VK_TRUE;
      enabled.core.features.variableMultisampleRate               = supported.core.features.variableMultisampleRate;
      enabled.extTransformFeedback.transformFeedback              = VK_TRUE;
      enabled.extTransformFeedback.geometryStreams                = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_1) {
      enabled.core.features.dualSrcBlend                          = VK_TRUE;
      enabled.core.features.imageCubeArray                        = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) {
      enabled.core.features.drawIndirectFirstInstance             = VK_TRUE;
      enabled.core.features.fragmentStoresAndAtomics              = VK_TRUE;
      enabled.core.features.multiDrawIndirect                     = VK_TRUE;
      enabled.core.features.shaderFloat64                         = supported.core.features.shaderFloat64;
      enabled.core.features.shaderInt64                           = supported.core.features.shaderInt64;
      enabled.core.features.shaderStorageImageReadWithoutFormat   = supported.core.features.shaderStorageImageReadWithoutFormat;
      enabled.core.features.tessellationShader                    = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_11_1) {
      enabled.core.features.logicOp                               = VK_TRUE;
      enabled.core.features.variableMultisampleRate               = VK_TRUE;
      enabled.core.features.vertexPipelineStoresAndAtomics        = VK_TRUE;
    }
    
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
     && !m_dxvkDevice->extensions().extShaderStencilExport)
      return E_INVALIDARG;

    if (shader->flags().test(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage)
     && !m_dxvkDevice->extensions().extShaderViewportIndexLayer)
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
    if (Format != DXGI_FORMAT_UNKNOWN && fmtMapping.Format == VK_FORMAT_UNDEFINED)
      return E_FAIL;
    
    // Query Vulkan format properties and supported features for it
    const DxvkFormatInfo* fmtProperties = imageFormatInfo(fmtMapping.Format);

    VkFormatProperties fmtSupport = fmtMapping.Format != VK_FORMAT_UNDEFINED
      ? m_dxvkAdapter->formatProperties(fmtMapping.Format)
      : VkFormatProperties();
    
    VkFormatFeatureFlags bufFeatures = fmtSupport.bufferFeatures;
    VkFormatFeatureFlags imgFeatures = fmtSupport.optimalTilingFeatures | fmtSupport.linearTilingFeatures;

    // For multi-plane images, we want to check available view formats as well
    if (fmtProperties->flags.test(DxvkFormatFlag::MultiPlane)) {
      const VkFormatFeatureFlags featureMask
        = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
        | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      DXGI_VK_FORMAT_FAMILY formatFamily = LookupFamily(Format, DXGI_VK_FORMAT_MODE_ANY);

      for (uint32_t i = 0; i < formatFamily.FormatCount; i++) {
        VkFormatProperties viewFmtSupport = m_dxvkAdapter->formatProperties(formatFamily.Formats[i]);
        imgFeatures |= (viewFmtSupport.optimalTilingFeatures | viewFmtSupport.linearTilingFeatures) & featureMask;
      }
    }
    
    UINT flags1 = 0;
    UINT flags2 = 0;

    // Format can be used for shader resource views with buffers
    if (bufFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT
     || Format == DXGI_FORMAT_UNKNOWN)
      flags1 |= D3D11_FORMAT_SUPPORT_BUFFER;
    
    // Format can be used for vertex data
    if (bufFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
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
    
    if (imgFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
     || imgFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
      const VkFormat depthFormat = LookupFormat(Format, DXGI_VK_FORMAT_MODE_DEPTH).Format;
      
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_1D)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE1D;
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_2D)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE2D;
      if (GetImageTypeSupport(fmtMapping.Format, VK_IMAGE_TYPE_3D)) flags1 |= D3D11_FORMAT_SUPPORT_TEXTURE3D;
      
      flags1 |= D3D11_FORMAT_SUPPORT_MIP
             |  D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT;
    
      // Format can be read 
      if (imgFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
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
      if (imgFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
        flags1 |= D3D11_FORMAT_SUPPORT_RENDER_TARGET
               |  D3D11_FORMAT_SUPPORT_MIP_AUTOGEN
               |  D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
        
        if (m_dxvkDevice->features().core.features.logicOp)
          flags2 |= D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP;
      }
      
      // Format supports blending when used for rendering
      if (imgFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
        flags1 |= D3D11_FORMAT_SUPPORT_BLENDABLE;
      
      // Format is a depth-stencil format that can be used for rendering
      if (imgFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        flags1 |= D3D11_FORMAT_SUPPORT_DEPTH_STENCIL;
      
      // FIXME implement properly. This would require a VkSurface.
      if (Format == DXGI_FORMAT_R8G8B8A8_UNORM
       || Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
       || Format == DXGI_FORMAT_B8G8R8A8_UNORM
       || Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
       || Format == DXGI_FORMAT_R16G16B16A16_FLOAT
       || Format == DXGI_FORMAT_R10G10B10A2_UNORM
       || Format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)
        flags1 |= D3D11_FORMAT_SUPPORT_DISPLAY;
      
      // Query multisample support for this format
      VkImageFormatProperties imgFmtProperties;
      
      VkResult status = m_dxvkAdapter->imageFormatProperties(fmtMapping.Format,
        VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        (fmtProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
          ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
          : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        0, imgFmtProperties);
      
      if (status == VK_SUCCESS && imgFmtProperties.sampleCounts > VK_SAMPLE_COUNT_1_BIT) {
        flags1 |= D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
               |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE
               |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD;
      }
    }
    
    // Format can be used for storage images or storage texel buffers
    if ((bufFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT)
     && (imgFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
      flags1 |= D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW;
      flags2 |= D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
      
      if (m_dxvkDevice->features().core.features.shaderStorageImageReadWithoutFormat
       || Format == DXGI_FORMAT_R32_UINT
       || Format == DXGI_FORMAT_R32_SINT
       || Format == DXGI_FORMAT_R32_FLOAT)
        flags2 |= D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
      
      if (Format == DXGI_FORMAT_R32_UINT
       || Format == DXGI_FORMAT_R32_SINT) {
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
  
  
  BOOL D3D11Device::GetImageTypeSupport(VkFormat Format, VkImageType Type) const {
    VkImageFormatProperties props;
    
    VkResult status = m_dxvkAdapter->imageFormatProperties(
      Format, Type, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT, 0, props);
    
    if (status != VK_SUCCESS) {
      status = m_dxvkAdapter->imageFormatProperties(
        Format, Type, VK_IMAGE_TILING_LINEAR,
        VK_IMAGE_USAGE_SAMPLED_BIT, 0, props);
    }
    
    return status == VK_SUCCESS;
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


  template<typename Void>
  void D3D11Device::CopySubresourceData(
          Void*                       pData,
          UINT                        RowPitch,
          UINT                        DepthPitch,
          ID3D11Resource*             pResource,
          UINT                        Subresource,
    const D3D11_BOX*                  pBox) {
    auto texture = GetCommonTexture(pResource);

    if (!texture)
      return;
    
    // Validate texture state and skip invalid calls
    if (texture->Desc()->Usage != D3D11_USAGE_DEFAULT
     || texture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_NONE
     || texture->CountSubresources() <= Subresource
     || texture->GetMapType(Subresource) == D3D11_MAP(~0u))
      return;

    // Retrieve image format information
    VkFormat packedFormat = LookupPackedFormat(
      texture->Desc()->Format,
      texture->GetFormatMode()).Format;
    
    auto formatInfo = imageFormatInfo(packedFormat);
    
    // Validate box against subresource dimensions
    Rc<DxvkImage> image = texture->GetImage();

    auto subresource = texture->GetSubresourceFromIndex(
      formatInfo->aspectMask, Subresource);
    
    VkOffset3D offset = { 0, 0, 0 };
    VkExtent3D extent = image->mipLevelExtent(subresource.mipLevel);

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

    // We can only operate on full blocks of compressed images
    offset = util::computeBlockOffset(offset, formatInfo->blockSize);
    extent = util::computeBlockCount(extent, formatInfo->blockSize);

    // Determine the memory layout of the image data
    D3D11_MAPPED_SUBRESOURCE subresourceData = { };

    if (texture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      VkSubresourceLayout layout = image->querySubresourceLayout(subresource);
      subresourceData.pData      = image->mapPtr(layout.offset);
      subresourceData.RowPitch   = layout.rowPitch;
      subresourceData.DepthPitch = layout.depthPitch;
    } else {
      subresourceData.pData      = texture->GetMappedBuffer(Subresource)->mapPtr(0);
      subresourceData.RowPitch   = formatInfo->elementSize * extent.width;
      subresourceData.DepthPitch = formatInfo->elementSize * extent.width * extent.height;
    }

    if constexpr (std::is_const<Void>::value) {
      // WriteToSubresource
      auto src = reinterpret_cast<const char*>(pData);
      auto dst = reinterpret_cast<      char*>(subresourceData.pData);

      for (uint32_t z = 0; z < extent.depth; z++) {
        for (uint32_t y = 0; y < extent.height; y++) {
          std::memcpy(
            dst + (offset.z + z) * subresourceData.DepthPitch
                + (offset.y + y) * subresourceData.RowPitch
                + (offset.x)     * formatInfo->elementSize,
            src + z * DepthPitch
                + y * RowPitch,
            formatInfo->elementSize * extent.width);
        }
      }
    } else {
      // ReadFromSubresource
      auto src = reinterpret_cast<const char*>(subresourceData.pData);
      auto dst = reinterpret_cast<      char*>(pData);

      for (uint32_t z = 0; z < extent.depth; z++) {
        for (uint32_t y = 0; y < extent.height; y++) {
          std::memcpy(
            dst + z * DepthPitch
                + y * RowPitch,
            src + (offset.z + z) * subresourceData.DepthPitch
                + (offset.y + y) * subresourceData.RowPitch
                + (offset.x)     * formatInfo->elementSize,
            formatInfo->elementSize * extent.width);
        }
      }
    }
  }


  D3D_FEATURE_LEVEL D3D11Device::GetMaxFeatureLevel(const Rc<DxvkInstance>& pInstance) {
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
    
    const std::string maxLevel = pInstance->config()
      .getOption<std::string>("d3d11.maxFeatureLevel");
    
    auto entry = std::find_if(s_featureLevels.begin(), s_featureLevels.end(),
      [&] (const std::pair<std::string, D3D_FEATURE_LEVEL>& pair) {
        return pair.first == maxLevel;
      });
    
    return entry != s_featureLevels.end()
      ? entry->second
      : D3D_FEATURE_LEVEL_11_1;
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
    const auto& deviceExtensions = m_device->GetDXVKDevice()->extensions();
    
    switch (Extension) {
      case D3D11_VK_EXT_BARRIER_CONTROL:
        return true;
      
      case D3D11_VK_EXT_MULTI_DRAW_INDIRECT:
        return deviceFeatures.core.features.multiDrawIndirect;
        
      case D3D11_VK_EXT_MULTI_DRAW_INDIRECT_COUNT:
        return deviceFeatures.core.features.multiDrawIndirect
            && deviceExtensions.khrDrawIndirectCount;
      
      case D3D11_VK_EXT_DEPTH_BOUNDS:
        return deviceFeatures.core.features.depthBounds;

      default:
        return false;
    }
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




  WineDXGISwapChainFactory::WineDXGISwapChainFactory(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice)
  : m_container(pContainer), m_device(pDevice) {
    
  }
  
  
  ULONG STDMETHODCALLTYPE WineDXGISwapChainFactory::AddRef() {
    return m_device->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE WineDXGISwapChainFactory::Release() {
    return m_device->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE WineDXGISwapChainFactory::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE WineDXGISwapChainFactory::CreateSwapChainForHwnd(
          IDXGIFactory*           pFactory,
          HWND                    hWnd,
    const DXGI_SWAP_CHAIN_DESC1*  pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
          IDXGIOutput*            pRestrictToOutput,
          IDXGISwapChain1**       ppSwapChain) {
    InitReturnPtr(ppSwapChain);
    
    if (!ppSwapChain || !pDesc || !hWnd)
      return DXGI_ERROR_INVALID_CALL;
    
    // Make sure the back buffer size is not zero
    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
    
    GetWindowClientSize(hWnd,
      desc.Width  ? nullptr : &desc.Width,
      desc.Height ? nullptr : &desc.Height);
    
    // If necessary, set up a default set of
    // fullscreen parameters for the swap chain
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
    
    if (pFullscreenDesc) {
      fsDesc = *pFullscreenDesc;
    } else {
      fsDesc.RefreshRate      = { 0, 0 };
      fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
      fsDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
      fsDesc.Windowed         = TRUE;
    }
    
    try {
      // Create presenter for the device
      Com<D3D11SwapChain> presenter = new D3D11SwapChain(
        m_container, m_device, hWnd, &desc);
      
      // Create the actual swap chain
      *ppSwapChain = ref(new DxgiSwapChain(
        pFactory, presenter.ptr(), hWnd, &desc, &fsDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
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
    const Rc<DxvkInstance>&   pDxvkInstance,
    const Rc<DxvkAdapter>&    pDxvkAdapter,
          D3D_FEATURE_LEVEL   FeatureLevel,
          UINT                FeatureFlags)
  : m_dxgiAdapter   (pAdapter),
    m_dxvkInstance  (pDxvkInstance),
    m_dxvkAdapter   (pDxvkAdapter),
    m_dxvkDevice    (CreateDevice(FeatureLevel)),
    m_d3d11Device   (this, FeatureLevel, FeatureFlags),
    m_d3d11DeviceExt(this, &m_d3d11Device),
    m_d3d11Interop  (this, &m_d3d11Device),
    m_d3d11Video    (this, &m_d3d11Device),
    m_metaDevice    (this),
    m_wineFactory   (this, &m_d3d11Device) {

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
    
    if (riid == __uuidof(ID3D11VkExtDevice)) {
      *ppvObject = ref(&m_d3d11DeviceExt);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIDXVKDevice)) {
      *ppvObject = ref(&m_metaDevice);
      return S_OK;
    }

    if (riid == __uuidof(IWineDXGISwapChainFactory)) {
      *ppvObject = ref(&m_wineFactory);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11VideoDevice)) {
      *ppvObject = ref(&m_d3d11Video);
      return S_OK;
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
    
    Logger::warn("D3D11DXGIDevice::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
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
    Logger::err("D3D11DXGIDevice::EnqueueSetEvent: Not implemented");
    return DXGI_ERROR_UNSUPPORTED;           
  }


  void STDMETHODCALLTYPE D3D11DXGIDevice::Trim() {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11DXGIDevice::Trim: Stub");
  }
  
  
  Rc<DxvkDevice> STDMETHODCALLTYPE D3D11DXGIDevice::GetDXVKDevice() {
    return m_dxvkDevice;
  }


  Rc<DxvkDevice> D3D11DXGIDevice::CreateDevice(D3D_FEATURE_LEVEL FeatureLevel) {
    DxvkDeviceFeatures deviceFeatures = D3D11Device::GetDeviceFeatures(m_dxvkAdapter, FeatureLevel);
    return m_dxvkAdapter->createDevice(m_dxvkInstance, deviceFeatures);
  }

}
