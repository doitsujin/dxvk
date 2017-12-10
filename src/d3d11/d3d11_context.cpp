#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {
  
  D3D11DeviceContext::D3D11DeviceContext(
      ID3D11Device*   parent,
      Rc<DxvkDevice>  device)
  : m_parent(parent),
    m_device(device) {
    m_context = m_device->createContext();
    m_context->beginRecording(
      m_device->createCommandList());
    this->SetDefaultBlendState();
    this->SetDefaultDepthStencilState();
    this->SetDefaultRasterizerState();
  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  HRESULT D3D11DeviceContext::QueryInterface(
          REFIID  riid,
          void**  ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceContext);
    
    Logger::warn("D3D11DeviceContext::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11DeviceContext::GetDevice(ID3D11Device **ppDevice) {
    *ppDevice = ref(m_parent);
  }
  
  
  D3D11_DEVICE_CONTEXT_TYPE D3D11DeviceContext::GetType() {
    return m_type;
  }
  
  
  UINT D3D11DeviceContext::GetContextFlags() {
    return m_flags;
  }
  
  
  void D3D11DeviceContext::ClearState() {
    this->IASetInputLayout(nullptr);
    this->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    this->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, nullptr, nullptr, nullptr);
    this->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    
    this->VSSetShader(nullptr, nullptr, 0);
    this->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
    this->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
    this->VSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
//     this->HSSetShader(nullptr, nullptr, 0);
//     this->HSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
//     this->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
//     this->HSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
//     this->DSSetShader(nullptr, nullptr, 0);
//     this->DSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
//     this->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
//     this->DSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
//     this->GSSetShader(nullptr, nullptr, 0);
//     this->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
//     this->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
//     this->GSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
    this->PSSetShader(nullptr, nullptr, 0);
    this->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
    this->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
    this->PSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
//     this->CSSetShader(nullptr, nullptr, 0);
//     this->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, nullptr);
//     this->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullptr);
//     this->CSSetSamplers       (0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, nullptr);
    
    this->OMSetRenderTargets(0, nullptr, nullptr);
    this->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
    this->OMSetDepthStencilState(nullptr, 0);
    
    this->RSSetState(nullptr);
    this->RSSetViewports(0, nullptr);
    this->RSSetScissorRects(0, nullptr);
    
//     this->SOSetTargets(0, nullptr, nullptr);
    
//     this->SetPredication(nullptr, FALSE);
  }
  
  
  void D3D11DeviceContext::Flush() {
    if (m_type == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
      m_device->submitCommandList(
        m_context->endRecording(),
        nullptr, nullptr);
      
      m_context->beginRecording(
        m_device->createCommandList());
    } else {
      Logger::err("D3D11DeviceContext::Flush: Not supported on deferred context");
    }
  }
  
  
  void D3D11DeviceContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          WINBOOL             RestoreContextState) {
    Logger::err("D3D11DeviceContext::ExecuteCommandList: Not implemented");
  }
  
  
  HRESULT D3D11DeviceContext::FinishCommandList(
          WINBOOL             RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    if (m_type == D3D11_DEVICE_CONTEXT_DEFERRED) {
      Logger::err("D3D11DeviceContext::FinishCommandList: Not implemented");
      return E_NOTIMPL;
    } else {
      Logger::err("D3D11DeviceContext::FinishCommandList: Not supported on immediate context");
      return DXGI_ERROR_INVALID_CALL;
    }
  }
  
  
  HRESULT D3D11DeviceContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    Logger::err("D3D11DeviceContext::Map: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void D3D11DeviceContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    Logger::err("D3D11DeviceContext::Unmap: Not implemented");
  }
  
  
  void D3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync) {
    Logger::err("D3D11DeviceContext::Begin: Not implemented");
  }
  
  
  void D3D11DeviceContext::End(ID3D11Asynchronous *pAsync) {
    Logger::err("D3D11DeviceContext::End: Not implemented");
  }
  
  
  HRESULT D3D11DeviceContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    Logger::err("D3D11DeviceContext::GetData: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void D3D11DeviceContext::SetPredication(
          ID3D11Predicate*                  pPredicate,
          WINBOOL                           PredicateValue) {
    Logger::err("D3D11DeviceContext::SetPredication: Not implemented");
  }
  
  
  void D3D11DeviceContext::GetPredication(
          ID3D11Predicate**                 ppPredicate,
          WINBOOL*                          pPredicateValue) {
    Logger::err("D3D11DeviceContext::GetPredication: Not implemented");
  }
  
  
  void D3D11DeviceContext::CopySubresourceRegion(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox) {
    Logger::err("D3D11DeviceContext::CopySubresourceRegion: Not implemented");
  }
  
  
  void D3D11DeviceContext::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    Logger::err("D3D11DeviceContext::CopyResource: Not implemented");
  }
  
  
  void D3D11DeviceContext::CopyStructureCount(
          ID3D11Buffer*                     pDstBuffer,
          UINT                              DstAlignedByteOffset,
          ID3D11UnorderedAccessView*        pSrcView) {
    Logger::err("D3D11DeviceContext::CopyStructureCount: Not implemented");
  }
  
  
  void D3D11DeviceContext::ClearRenderTargetView(
          ID3D11RenderTargetView*           pRenderTargetView,
    const FLOAT                             ColorRGBA[4]) {
    auto rtv = static_cast<D3D11RenderTargetView*>(pRenderTargetView);
    const Rc<DxvkImageView> dxvkView = rtv->GetDXVKImageView();
    
    // Find out whether the given attachment is currently bound
    // or not, and if it is, which attachment index it has.
    int32_t attachmentIndex = -1;
    
    for (uint32_t i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      if (m_state.om.renderTargetViews.at(i) == rtv)
        attachmentIndex = i;
    }
    
    // Copy the clear color into a clear value structure.
    // This should also work for images that don nott have
    // a floating point format.
    VkClearColorValue clearValue;
    std::memcpy(clearValue.float32, ColorRGBA,
      sizeof(clearValue.float32));
    
    if (attachmentIndex >= 0) {
      // Image is bound to the pipeline for rendering. We can
      // use the clear function that operates on attachments.
      VkClearAttachment clearInfo;
      clearInfo.aspectMask          = VK_IMAGE_ASPECT_COLOR_BIT;
      clearInfo.colorAttachment     = static_cast<uint32_t>(attachmentIndex);
      clearInfo.clearValue.color    = clearValue;
      
      // Clear the full area. On FL 9.x, only the first array
      // layer will be cleared, rather than all array layers.
      VkClearRect clearRect;
      clearRect.rect.offset.x       = 0;
      clearRect.rect.offset.y       = 0;
      clearRect.rect.extent.width   = dxvkView->imageInfo().extent.width;
      clearRect.rect.extent.height  = dxvkView->imageInfo().extent.height;
      clearRect.baseArrayLayer      = 0;
      clearRect.layerCount          = dxvkView->imageInfo().numLayers;
      
      if (m_parent->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        clearRect.layerCount        = 1;
      
      m_context->clearRenderTarget(clearInfo, clearRect);
    } else {
      // Image is not bound to the pipeline. We can still clear
      // it, but we'll have to use a generic clear function.
      m_context->clearColorImage(dxvkView->image(),
        clearValue, dxvkView->subresources());
    }
  }
  
  
  void D3D11DeviceContext::ClearUnorderedAccessViewUint(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const UINT                              Values[4]) {
    Logger::err("D3D11DeviceContext::ClearUnorderedAccessViewUint: Not implemented");
  }
  
  
  void D3D11DeviceContext::ClearUnorderedAccessViewFloat(
          ID3D11UnorderedAccessView*        pUnorderedAccessView,
    const FLOAT                             Values[4]) {
    Logger::err("D3D11DeviceContext::ClearUnorderedAccessViewFloat: Not implemented");
  }
  
  
  void D3D11DeviceContext::ClearDepthStencilView(
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    auto dsv = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
    const Rc<DxvkImageView> dxvkView = dsv->GetDXVKImageView();
    
    VkClearDepthStencilValue clearValue;
    clearValue.depth   = Depth;
    clearValue.stencil = Stencil;
    
    if (m_state.om.depthStencilView == dsv) {
      // Image is bound to the pipeline for rendering. We can
      // use the clear function that operates on attachments.
      VkClearAttachment clearInfo;
      clearInfo.aspectMask              = 0;
      clearInfo.colorAttachment         = 0;
      clearInfo.clearValue.depthStencil = clearValue;
      
      if (ClearFlags & D3D11_CLEAR_DEPTH)
        clearInfo.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
      
      if (ClearFlags & D3D11_CLEAR_STENCIL)
        clearInfo.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      
      // Clear the full area
      VkClearRect clearRect;
      clearRect.rect.offset.x       = 0;
      clearRect.rect.offset.y       = 0;
      clearRect.rect.extent.width   = dxvkView->imageInfo().extent.width;
      clearRect.rect.extent.height  = dxvkView->imageInfo().extent.height;
      clearRect.baseArrayLayer      = 0;
      clearRect.layerCount          = dxvkView->imageInfo().numLayers;
      
      // FIXME Is this correct? Docs don't say anything
      if (m_parent->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        clearRect.layerCount        = 1;
      
      m_context->clearRenderTarget(clearInfo, clearRect);
    } else {
      m_context->clearDepthStencilImage(dxvkView->image(),
        clearValue, dxvkView->subresources());
    }
  }
  
  
  void D3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) {
    Logger::err("D3D11DeviceContext::GenerateMips: Not implemented");
  }
  
  
  void D3D11DeviceContext::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    // We need a different code path for buffers
    D3D11_RESOURCE_DIMENSION resourceType;
    pDstResource->GetType(&resourceType);
    
    if (resourceType == D3D11_RESOURCE_DIMENSION_BUFFER) {
      Com<IDXGIBufferResourcePrivate> bufferResource;
      
      pDstResource->QueryInterface(
        __uuidof(IDXGIBufferResourcePrivate),
        reinterpret_cast<void**>(&bufferResource));
      
      VkDeviceSize offset = 0;
      VkDeviceSize size = VK_WHOLE_SIZE;
      
      if (pDstBox != nullptr) {
        offset = pDstBox->left;
        size   = pDstBox->right - pDstBox->left;
      }
      
      m_context->updateBuffer(
        bufferResource->GetDXVKBuffer(),
        offset, size, pSrcData);
    } else {
      Logger::err("D3D11DeviceContext::UpdateSubresource: Images not yet supported");
    }
    
  }
  
  
  void D3D11DeviceContext::SetResourceMinLOD(
          ID3D11Resource*                   pResource,
          FLOAT                             MinLOD) {
    Logger::err("D3D11DeviceContext::SetResourceMinLOD: Not implemented");
  }
  
  
  FLOAT D3D11DeviceContext::GetResourceMinLOD(ID3D11Resource* pResource) {
    Logger::err("D3D11DeviceContext::GetResourceMinLOD: Not implemented");
    return 0.0f;
  }
  
  
  void D3D11DeviceContext::ResolveSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    Logger::err("D3D11DeviceContext::ResolveSubresource: Not implemented");
  }
  
  
  void D3D11DeviceContext::DrawAuto() {
    Logger::err("D3D11DeviceContext::DrawAuto: Not implemented");
  }
  
  
  void D3D11DeviceContext::Draw(
          UINT            VertexCount,
          UINT            StartVertexLocation) {
    m_context->draw(
      VertexCount, 1,
      StartVertexLocation, 0);
  }
  
  
  void D3D11DeviceContext::DrawIndexed(
          UINT            IndexCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation) {
    m_context->drawIndexed(
      IndexCount, 1,
      StartIndexLocation,
      BaseVertexLocation, 0);
  }
  
  
  void D3D11DeviceContext::DrawInstanced(
          UINT            VertexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartVertexLocation,
          UINT            StartInstanceLocation) {
    m_context->draw(
      VertexCountPerInstance,
      InstanceCount,
      StartVertexLocation,
      StartInstanceLocation);
  }
  
  
  void D3D11DeviceContext::DrawIndexedInstanced(
          UINT            IndexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation,
          UINT            StartInstanceLocation) {
    m_context->drawIndexed(
      IndexCountPerInstance,
      InstanceCount,
      StartIndexLocation,
      BaseVertexLocation,
      StartInstanceLocation);
  }
  
  
  void D3D11DeviceContext::DrawIndexedInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    Logger::err("D3D11DeviceContext::DrawIndexedInstancedIndirect: Not implemented");
  }
  
  
  void D3D11DeviceContext::DrawInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    Logger::err("D3D11DeviceContext::DrawInstancedIndirect: Not implemented");
  }
  
  
  void D3D11DeviceContext::Dispatch(
          UINT            ThreadGroupCountX,
          UINT            ThreadGroupCountY,
          UINT            ThreadGroupCountZ) {
    m_context->dispatch(
      ThreadGroupCountX,
      ThreadGroupCountY,
      ThreadGroupCountZ);
  }
  
  
  void D3D11DeviceContext::DispatchIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    Logger::err("D3D11DeviceContext::DispatchIndirect: Not implemented");
  }
  
  
  void D3D11DeviceContext::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
    auto inputLayout = static_cast<D3D11InputLayout*>(pInputLayout);
    
    if (m_state.ia.inputLayout != inputLayout) {
      m_state.ia.inputLayout = inputLayout;
      
      if (inputLayout != nullptr)
        inputLayout->BindToContext(m_context);
      else
        m_context->setInputLayout(0, nullptr, 0, nullptr);
    }
  }
  
  
  void D3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    if (m_state.ia.primitiveTopology != Topology) {
      m_state.ia.primitiveTopology = Topology;
      
      DxvkInputAssemblyState iaState;
      
      switch (Topology) {
        case D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED:
          return;
        
        case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
          iaState.primitiveRestart  = VK_FALSE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
          iaState.primitiveRestart  = VK_FALSE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
          iaState.primitiveRestart  = VK_TRUE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
          iaState.primitiveRestart  = VK_FALSE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
          iaState.primitiveRestart  = VK_TRUE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
          iaState.primitiveRestart  = VK_FALSE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
          iaState.primitiveRestart  = VK_TRUE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
          iaState.primitiveRestart  = VK_FALSE;
          break;
        
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
          iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
          iaState.primitiveRestart  = VK_TRUE;
          break;
        
        default:
          Logger::err(str::format(
            "D3D11DeviceContext::IASetPrimitiveTopology: Unknown primitive topology: ",
            Topology));
      }
      
      m_context->setInputAssemblyState(iaState);
    }
  }
  
  
  void D3D11DeviceContext::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    // TODO check if any of these buffers
    // are bound as UAVs or stream outputs
    for (uint32_t i = 0; i < NumBuffers; i++) {
      D3D11VertexBufferBinding binding;
      binding.buffer = nullptr;
      binding.offset = 0;
      binding.stride = 0;
      m_state.ia.vertexBuffers.at(StartSlot + i) = binding;
      
      if (ppVertexBuffers != nullptr) {
        binding.buffer = static_cast<D3D11Buffer*>(ppVertexBuffers[i]);
        binding.offset = pOffsets[i];
        binding.stride = pStrides[i];
      }
      
      DxvkBufferBinding dxvkBinding;
      
      if (binding.buffer != nullptr) {
        Rc<DxvkBuffer> dxvkBuffer = binding.buffer->GetDXVKBuffer();
        
        dxvkBinding = DxvkBufferBinding(
          dxvkBuffer, binding.offset,
          dxvkBuffer->info().size - binding.offset);
      }
      
      m_context->bindVertexBuffer(
        StartSlot + i, dxvkBinding,
        binding.stride);
    }
  }
  
  
  void D3D11DeviceContext::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    D3D11IndexBufferBinding binding;
    binding.buffer = static_cast<D3D11Buffer*>(pIndexBuffer);
    binding.offset = Offset;
    binding.format = Format;
    m_state.ia.indexBuffer = binding;
    
    DxvkBufferBinding dxvkBinding;
    
    if (binding.buffer != nullptr) {
      Rc<DxvkBuffer> dxvkBuffer = binding.buffer->GetDXVKBuffer();
      
      dxvkBinding = DxvkBufferBinding(
        dxvkBuffer, binding.offset,
        dxvkBuffer->info().size - binding.offset);
    }
    
    
    // As in Vulkan, the index format can be either a 32-bit
    // unsigned integer or a 16-bit unsigned integer, no other
    // formats are allowed.
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    
    if (binding.buffer != nullptr) {
      switch (binding.format) {
        case DXGI_FORMAT_R16_UINT: indexType = VK_INDEX_TYPE_UINT16; break;
        case DXGI_FORMAT_R32_UINT: indexType = VK_INDEX_TYPE_UINT32; break;
        default: Logger::err(str::format("D3D11: Invalid index format: ", binding.format));
      }
    }
    
    m_context->bindIndexBuffer(
      dxvkBinding, indexType);
  }
  
  
  void D3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) {
    *ppInputLayout = m_state.ia.inputLayout.ref();
  }
  
  
  void D3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) {
    *pTopology = m_state.ia.primitiveTopology;
  }
  
  
  void D3D11DeviceContext::IAGetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppVertexBuffers,
          UINT*                             pStrides,
          UINT*                             pOffsets) {
    Logger::err("D3D11DeviceContext::IAGetVertexBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::IAGetIndexBuffer(
          ID3D11Buffer**                    pIndexBuffer,
          DXGI_FORMAT*                      Format,
          UINT*                             Offset) {
    Logger::err("D3D11DeviceContext::IAGetIndexBuffer: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSSetShader(
          ID3D11VertexShader*               pVertexShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11VertexShader*>(pVertexShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::VSSetShader: Class instances not supported");
    
    if (m_state.vs.shader != shader) {
      m_state.vs.shader = shader;
      
      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,
        shader != nullptr ? shader->GetShader() : nullptr);
    }
  }
  
  
  void D3D11DeviceContext::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::VertexShader,
      &m_state.vs.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void D3D11DeviceContext::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::VertexShader,
      &m_state.vs.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void D3D11DeviceContext::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::VertexShader,
      &m_state.vs.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void D3D11DeviceContext::VSGetShader(
          ID3D11VertexShader**              ppVertexShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::VSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::VSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::VSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::VSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSSetShader(
          ID3D11HullShader*                 pHullShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    Logger::err("D3D11DeviceContext::HSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::HSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::HSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::HSSetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSGetShader(
          ID3D11HullShader**                ppHullShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::HSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::HSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::HSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::HSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::HSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSSetShader(
          ID3D11DomainShader*               pDomainShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    Logger::err("D3D11DeviceContext::DSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::DSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::DSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::DSSetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSGetShader(
          ID3D11DomainShader**              ppDomainShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::DSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::DSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::DSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::DSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::DSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSSetShader(
          ID3D11GeometryShader*             pShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    Logger::err("D3D11DeviceContext::GSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::GSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::GSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::GSSetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSGetShader(
          ID3D11GeometryShader**            ppGeometryShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::GSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::GSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::GSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::GSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSSetShader(
          ID3D11PixelShader*                pPixelShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    auto shader = static_cast<D3D11PixelShader*>(pPixelShader);
    
    if (NumClassInstances != 0)
      Logger::err("D3D11DeviceContext::PSSetShader: Class instances not supported");
    
    if (m_state.ps.shader != shader) {
      m_state.ps.shader = shader;
      
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT,
        shader != nullptr ? shader->GetShader() : nullptr);
    }
  }
  
  
  void D3D11DeviceContext::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    this->BindConstantBuffers(
      DxbcProgramType::PixelShader,
      &m_state.ps.constantBuffers,
      StartSlot, NumBuffers,
      ppConstantBuffers);
  }
  
  
  void D3D11DeviceContext::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    this->BindShaderResources(
      DxbcProgramType::PixelShader,
      &m_state.ps.shaderResources,
      StartSlot, NumViews,
      ppShaderResourceViews);
  }
  
  
  void D3D11DeviceContext::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    this->BindSamplers(
      DxbcProgramType::PixelShader,
      &m_state.ps.samplers,
      StartSlot, NumSamplers,
      ppSamplers);
  }
  
  
  void D3D11DeviceContext::PSGetShader(
          ID3D11PixelShader**               ppPixelShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::PSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::PSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::PSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::PSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSSetShader(
          ID3D11ComputeShader*              pComputeShader,
          ID3D11ClassInstance* const*       ppClassInstances,
          UINT                              NumClassInstances) {
    Logger::err("D3D11DeviceContext::CSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::CSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::CSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::CSSetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSSetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    Logger::err("D3D11DeviceContext::CSSetUnorderedAccessViews: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSGetShader(
          ID3D11ComputeShader**             ppComputeShader,
          ID3D11ClassInstance**             ppClassInstances,
          UINT*                             pNumClassInstances) {
    Logger::err("D3D11DeviceContext::CSGetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::CSGetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView**        ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::CSGetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState**              ppSamplers) {
    Logger::err("D3D11DeviceContext::CSGetSamplers: Not implemented");
  }
  
  
  void D3D11DeviceContext::CSGetUnorderedAccessViews(
          UINT                              StartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    Logger::err("D3D11DeviceContext::CSGetUnorderedAccessViews: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
      D3D11RenderTargetView* view = nullptr;
      
      if ((i < NumViews) && (ppRenderTargetViews[i] != nullptr))
        view = static_cast<D3D11RenderTargetView*>(ppRenderTargetViews[i]);
      
      m_state.om.renderTargetViews.at(i) = view;
    }
    
    m_state.om.depthStencilView = static_cast<D3D11DepthStencilView*>(pDepthStencilView);
    
    
    // TODO unbind overlapping shader resource views
    
    Rc<DxvkFramebuffer> framebuffer = nullptr;
    
    if (ppRenderTargetViews != nullptr || pDepthStencilView != nullptr) {
      // D3D11 doesn't have the concept of a framebuffer object,
      // so we'll just create a new one every time the render
      // target bindings are updated. Set up the attachments.
      DxvkRenderTargets attachments;
      
      for (UINT i = 0; i < m_state.om.renderTargetViews.size(); i++) {
        if (m_state.om.renderTargetViews.at(i) != nullptr)
          attachments.setColorTarget(i, m_state.om.renderTargetViews.at(i)->GetDXVKImageView());
      }
      
      if (m_state.om.depthStencilView != nullptr)
        attachments.setDepthTarget(m_state.om.depthStencilView->GetDXVKImageView());
      
      framebuffer = m_device->createFramebuffer(attachments);
    }
    
    // Bind the framebuffer object to the context
    m_context->bindFramebuffer(framebuffer);
  }
  
  
  void D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    Logger::err("D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMSetBlendState(
          ID3D11BlendState*                 pBlendState,
    const FLOAT                             BlendFactor[4],
          UINT                              SampleMask) {
    Logger::err("D3D11DeviceContext::OMSetBlendState: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMSetDepthStencilState(
          ID3D11DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    Logger::err("D3D11DeviceContext::OMSetDepthStencilState: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    if (ppRenderTargetViews != nullptr) {
      for (UINT i = 0; i < NumViews; i++)
        ppRenderTargetViews[i] = i < m_state.om.renderTargetViews.size()
          ? m_state.om.renderTargetViews.at(i).ref()
          : nullptr;
    }
    
    if (ppDepthStencilView != nullptr) {
      Logger::err("D3D11DeviceContext::OMGetRenderTargets: Stencil view not supported");
      *ppDepthStencilView = nullptr;
    }
  }
  
  
  void D3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView**          ppRenderTargetViews,
          ID3D11DepthStencilView**          ppDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView**       ppUnorderedAccessViews) {
    Logger::err("D3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMGetBlendState(
          ID3D11BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    Logger::err("D3D11DeviceContext::OMGetBlendState: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMGetDepthStencilState(
          ID3D11DepthStencilState**         ppDepthStencilState,
          UINT*                             pStencilRef) {
    Logger::err("D3D11DeviceContext::OMGetDepthStencilState: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    auto rasterizerState = static_cast<D3D11RasterizerState*>(pRasterizerState);
    
    if (m_state.rs.state != rasterizerState) {
      m_state.rs.state = rasterizerState;
      
      if (rasterizerState != nullptr) {
        m_context->setRasterizerState(
          rasterizerState->GetDXVKRasterizerState());
      } else {
        // Restore the initial state
        this->SetDefaultRasterizerState();
      }
      
      // In D3D11, the rasterizer state defines
      // whether the scissor test is enabled, so
      // we have to update the scissor rectangles.
      this->ApplyViewportState();
    }
  }
  
  
  void D3D11DeviceContext::RSSetViewports(
          UINT                              NumViewports,
    const D3D11_VIEWPORT*                   pViewports) {
    m_state.rs.numViewports = NumViewports;
    
    for (uint32_t i = 0; i < NumViewports; i++)
      m_state.rs.viewports.at(i) = pViewports[i];
    
    this->ApplyViewportState();
  }
  
  
  void D3D11DeviceContext::RSSetScissorRects(
          UINT                              NumRects,
    const D3D11_RECT*                       pRects) {
    m_state.rs.numScissors = NumRects;
    
    for (uint32_t i = 0; i < NumRects; i++)
      m_state.rs.scissors.at(i) = pRects[i];
    
    if (m_state.rs.state != nullptr) {
      D3D11_RASTERIZER_DESC rsDesc;
      m_state.rs.state->GetDesc(&rsDesc);
      
      if (rsDesc.ScissorEnable)
        this->ApplyViewportState();
    }
  }
  
  
  void D3D11DeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    *ppRasterizerState = m_state.rs.state.ref();
  }
  
  
  void D3D11DeviceContext::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    if (pViewports != nullptr) {
      for (uint32_t i = 0; i < *pNumViewports; i++) {
        if (i < m_state.rs.numViewports) {
          pViewports[i] = m_state.rs.viewports.at(i);
        } else {
          pViewports[i].TopLeftX = 0.0f;
          pViewports[i].TopLeftY = 0.0f;
          pViewports[i].Width    = 0.0f;
          pViewports[i].Height   = 0.0f;
          pViewports[i].MinDepth = 0.0f;
          pViewports[i].MaxDepth = 0.0f;
        }
      }
    }
    
    *pNumViewports = m_state.rs.numViewports;
  }
  
  
  void D3D11DeviceContext::RSGetScissorRects(
          UINT*                             pNumRects,
          D3D11_RECT*                       pRects) {
    if (pRects != nullptr) {
      for (uint32_t i = 0; i < *pNumRects; i++) {
        if (i < m_state.rs.numScissors) {
          pRects[i] = m_state.rs.scissors.at(i);
        } else {
          pRects[i].left   = 0;
          pRects[i].top    = 0;
          pRects[i].right  = 0;
          pRects[i].bottom = 0;
        }
      }
    }
    
    *pNumRects = m_state.rs.numScissors;
  }
  
  
  void D3D11DeviceContext::SOSetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppSOTargets,
    const UINT*                             pOffsets) {
    Logger::err("D3D11DeviceContext::SOSetTargets: Not implemented");
  }
  
  
  void D3D11DeviceContext::SOGetTargets(
          UINT                              NumBuffers,
          ID3D11Buffer**                    ppSOTargets) {
    Logger::err("D3D11DeviceContext::SOGetTargets: Not implemented");
  }
  
  
  void D3D11DeviceContext::BindConstantBuffers(
          DxbcProgramType                   ShaderStage,
          D3D11ConstantBufferBindings*      pBindings,
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    for (uint32_t i = 0; i < NumBuffers; i++) {
      D3D11Buffer* buffer = nullptr;
      
      if (ppConstantBuffers != nullptr)
        buffer = static_cast<D3D11Buffer*>(ppConstantBuffers[i]);
      
      if (pBindings->at(StartSlot + i) != buffer) {
        pBindings->at(StartSlot + i) = buffer;
        
        // Figure out which part of the buffer to bind
        DxvkBufferBinding bindingInfo;
        
        if (buffer != nullptr) {
          bindingInfo = DxvkBufferBinding(
            buffer->GetDXVKBuffer(),
            0, VK_WHOLE_SIZE);
        }
        
        // Bind buffer to the DXVK resource slot
        const VkPipelineBindPoint bindPoint
          = ShaderStage == DxbcProgramType::ComputeShader
            ? VK_PIPELINE_BIND_POINT_COMPUTE
            : VK_PIPELINE_BIND_POINT_GRAPHICS;
        
        const uint32_t slotId = computeResourceSlotId(
          ShaderStage, DxbcBindingType::ConstantBuffer,
          StartSlot + i);
        
        m_context->bindResourceBuffer(
          bindPoint, slotId, bindingInfo);
      }
    }
  }
  
  
  void D3D11DeviceContext::BindSamplers(
          DxbcProgramType                   ShaderStage,
          D3D11SamplerBindings*             pBindings,
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    for (uint32_t i = 0; i < NumSamplers; i++) {
      D3D11SamplerState* sampler = nullptr;
      
      if (ppSamplers != nullptr)
        sampler = static_cast<D3D11SamplerState*>(ppSamplers[i]);
      
      if (pBindings->at(StartSlot + i) != sampler) {
        pBindings->at(StartSlot + i) = sampler;
        
        // Retrieve the DXVK sampler object
        Rc<DxvkSampler> samplerInfo = nullptr;
        
        if (sampler != nullptr)
          samplerInfo = sampler->GetDXVKSampler();
        
        // Bind sampler to the DXVK resource slot
        const VkPipelineBindPoint bindPoint
          = ShaderStage == DxbcProgramType::ComputeShader
            ? VK_PIPELINE_BIND_POINT_COMPUTE
            : VK_PIPELINE_BIND_POINT_GRAPHICS;
        
        const uint32_t slotId = computeResourceSlotId(
          ShaderStage, DxbcBindingType::ImageSampler,
          StartSlot + i);
        
        m_context->bindResourceSampler(
          bindPoint, slotId, samplerInfo);
      }
    }
  }
  
  
  void D3D11DeviceContext::BindShaderResources(
          DxbcProgramType                   ShaderStage,
          D3D11ShaderResourceBindings*      pBindings,
          UINT                              StartSlot,
          UINT                              NumResources,
          ID3D11ShaderResourceView* const*  ppResources) {
    for (uint32_t i = 0; i < NumResources; i++) {
      D3D11ShaderResourceView* resView = nullptr;
      
      if (ppResources != nullptr)
        resView = static_cast<D3D11ShaderResourceView*>(ppResources[i]);
      
      if (pBindings->at(StartSlot + i) != resView) {
        pBindings->at(StartSlot + i) = resView;
        
        // Bind sampler to the DXVK resource slot
        const VkPipelineBindPoint bindPoint
          = ShaderStage == DxbcProgramType::ComputeShader
            ? VK_PIPELINE_BIND_POINT_COMPUTE
            : VK_PIPELINE_BIND_POINT_GRAPHICS;
        
        const uint32_t slotId = computeResourceSlotId(
          ShaderStage, DxbcBindingType::ShaderResource,
          StartSlot + i);
        
        if (resView != nullptr) {
          // Figure out what we have to bind based on the resource type
          if (resView->GetResourceType() == D3D11_RESOURCE_DIMENSION_BUFFER) {
            Logger::warn("D3D11: Texel buffers not yet supported");
            m_context->bindResourceTexelBuffer(
              bindPoint, slotId, nullptr);
          } else {
            m_context->bindResourceImage(bindPoint,
              slotId, resView->GetDXVKImageView());
          }
        } else {
          // When unbinding a resource, it doesn't really matter if
          // the resource type is correct, so we'll just bind a null
          // image to the given resource slot
          m_context->bindResourceImage(
            bindPoint, slotId, nullptr);
        }
      }
    }
  }
  
  
  void D3D11DeviceContext::ApplyViewportState() {
    // We cannot set less than one viewport in Vulkan, and
    // rendering with no active viewport is illegal anyway.
    if (m_state.rs.numViewports == 0)
      return;
    
    std::array<VkViewport, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
    std::array<VkRect2D,   D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;
    
    // D3D11's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    for (uint32_t i = 0; i < m_state.rs.numViewports; i++) {
      const D3D11_VIEWPORT& vp = m_state.rs.viewports.at(i);
      
      viewports.at(i) = VkViewport {
        vp.TopLeftX, vp.Height - vp.TopLeftY,
        vp.Width,   -vp.Height,
        vp.MinDepth, vp.MaxDepth,
      };
    }
    
    // Scissor rectangles. Vulkan does not provide an easy way
    // to disable the scissor test, so we'll have to set scissor
    // rects that are at least as large as the framebuffer.
    bool enableScissorTest = false;
    
    if (m_state.rs.state != nullptr) {
      D3D11_RASTERIZER_DESC rsDesc;
      m_state.rs.state->GetDesc(&rsDesc);
      enableScissorTest = rsDesc.ScissorEnable;
    }
    
    for (uint32_t i = 0; i < m_state.rs.numViewports; i++) {
      // TODO D3D11 docs aren't clear about what should happen
      // when there are undefined scissor rects for a viewport.
      // Figure out what it does on Windows.
      if (enableScissorTest && (i < m_state.rs.numScissors)) {
        const D3D11_RECT& sr = m_state.rs.scissors.at(i);
        
        scissors.at(i) = VkRect2D {
          VkOffset2D { sr.left, sr.top },
          VkExtent2D {
            static_cast<uint32_t>(sr.right  - sr.left),
            static_cast<uint32_t>(sr.bottom - sr.top) } };
      } else {
        scissors.at(i) = VkRect2D {
          VkOffset2D { 0, 0 },
          VkExtent2D {
            D3D11_VIEWPORT_BOUNDS_MAX,
            D3D11_VIEWPORT_BOUNDS_MAX } };
      }
    }
    
    m_context->setViewports(
      m_state.rs.numViewports,
      viewports.data(),
      scissors.data());
  }
  
  
  void D3D11DeviceContext::SetDefaultBlendState() {
    DxvkMultisampleState msState;
    msState.enableAlphaToCoverage = VK_FALSE;
    msState.enableAlphaToOne      = VK_FALSE;
    msState.enableSampleShading   = VK_FALSE;
    msState.minSampleShading      = 0.0f;
    m_context->setMultisampleState(msState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_CLEAR;
    m_context->setLogicOpState(loState);
    
    DxvkBlendMode blendMode;
    blendMode.enableBlending = VK_FALSE;
    blendMode.colorSrcFactor = VK_BLEND_FACTOR_ONE;
    blendMode.colorDstFactor = VK_BLEND_FACTOR_ZERO;
    blendMode.colorBlendOp   = VK_BLEND_OP_ADD;
    blendMode.alphaSrcFactor = VK_BLEND_FACTOR_ONE;
    blendMode.alphaDstFactor = VK_BLEND_FACTOR_ZERO;
    blendMode.alphaBlendOp   = VK_BLEND_OP_ADD;
    blendMode.writeMask      = VK_COLOR_COMPONENT_R_BIT
                                      | VK_COLOR_COMPONENT_G_BIT
                                      | VK_COLOR_COMPONENT_B_BIT
                                      | VK_COLOR_COMPONENT_A_BIT;
    
    for (uint32_t i = 0; i < DxvkLimits::MaxNumRenderTargets; i++)
      m_context->setBlendMode(i, blendMode);
  }
  
  
  void D3D11DeviceContext::SetDefaultDepthStencilState() {
    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    stencilOp.writeMask   = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    stencilOp.reference   = 0;
    
    DxvkDepthStencilState dsState;
    dsState.enableDepthTest    = VK_TRUE;
    dsState.enableDepthWrite   = VK_TRUE;
    dsState.enableDepthBounds  = VK_FALSE;
    dsState.enableStencilTest  = VK_FALSE;
    dsState.depthCompareOp     = VK_COMPARE_OP_LESS;
    dsState.stencilOpFront     = stencilOp;
    dsState.stencilOpBack      = stencilOp;
    dsState.depthBoundsMin     = 0.0f;
    dsState.depthBoundsMax     = 1.0f;
    m_context->setDepthStencilState(dsState);
  }
  
  
  void D3D11DeviceContext::SetDefaultRasterizerState() {
    DxvkRasterizerState rsState;
    rsState.enableDepthClamp   = VK_FALSE;
    rsState.enableDiscard      = VK_FALSE;
    rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace          = VK_FRONT_FACE_CLOCKWISE;
    rsState.depthBiasEnable    = VK_FALSE;
    rsState.depthBiasConstant  = 0.0f;
    rsState.depthBiasClamp     = 0.0f;
    rsState.depthBiasSlope     = 0.0f;
    m_context->setRasterizerState(rsState);
  }
  
}
