#include "d3d11_context.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11DeviceContext::D3D11DeviceContext(
      ID3D11Device*   parent,
      Rc<DxvkDevice>  device)
  : m_parent(parent),
    m_device(device) {
    m_context = m_device->createContext();
    m_cmdList = m_device->createCommandList();
    m_context->beginRecording(m_cmdList);
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
    Logger::err("D3D11DeviceContext::ClearState: Not implemented");
  }
  
  
  void D3D11DeviceContext::Flush() {
    if (m_type == D3D11_DEVICE_CONTEXT_IMMEDIATE) {
      m_context->endRecording();
      m_device->submitCommandList(
        m_cmdList, nullptr, nullptr);
      
      m_cmdList = m_device->createCommandList();
      m_context->beginRecording(m_cmdList);
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
    Logger::err("D3D11DeviceContext::ClearRenderTargetView: Not implemented");
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
    Logger::err("D3D11DeviceContext::ClearDepthStencilView: Not implemented");
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
    Logger::err("D3D11DeviceContext::UpdateSubresource: Not implemented");
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
    Logger::err("D3D11DeviceContext::IASetInputLayout: Not implemented");
  }
  
  
  void D3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    Logger::err("D3D11DeviceContext::IASetPrimitiveTopology: Not implemented");
  }
  
  
  void D3D11DeviceContext::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    Logger::err("D3D11DeviceContext::IASetVertexBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::IASetIndexBuffer(
          ID3D11Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    Logger::err("D3D11DeviceContext::IASetIndexBuffer: Not implemented");
  }
  
  
  void D3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout** ppInputLayout) {
    Logger::err("D3D11DeviceContext::IAGetInputLayout: Not implemented");
  }
  
  
  void D3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) {
    Logger::err("D3D11DeviceContext::IAGetPrimitiveTopology: Not implemented");
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
    Logger::err("D3D11DeviceContext::VSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::VSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::VSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::VSSetSamplers: Not implemented");
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
    Logger::err("D3D11DeviceContext::PSSetShader: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D11Buffer* const*              ppConstantBuffers) {
    Logger::err("D3D11DeviceContext::PSSetConstantBuffers: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D11ShaderResourceView* const*  ppShaderResourceViews) {
    Logger::err("D3D11DeviceContext::PSSetShaderResources: Not implemented");
  }
  
  
  void D3D11DeviceContext::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D11SamplerState* const*        ppSamplers) {
    Logger::err("D3D11DeviceContext::PSSetSamplers: Not implemented");
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
    Logger::err("D3D11DeviceContext::OMSetRenderTargets: Not implemented");
  }
  
  
  void D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT* pUAVInitialCounts) {
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
    Logger::err("D3D11DeviceContext::OMGetRenderTargets: Not implemented");
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
    Logger::err("D3D11DeviceContext::RSSetState: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSSetViewports(
          UINT                              NumViewports,
    const D3D11_VIEWPORT*                   pViewports) {
    Logger::err("D3D11DeviceContext::RSSetViewports: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSSetScissorRects(
          UINT                              NumRects,
    const D3D11_RECT*                       pRects) {
    Logger::err("D3D11DeviceContext::RSSetScissorRects: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState) {
    Logger::err("D3D11DeviceContext::RSGetState: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSGetViewports(
          UINT*                             pNumViewports,
          D3D11_VIEWPORT*                   pViewports) {
    Logger::err("D3D11DeviceContext::RSGetViewports: Not implemented");
  }
  
  
  void D3D11DeviceContext::RSGetScissorRects(
          UINT*                             pNumRects,
          D3D11_RECT*                       pRects) {
    Logger::err("D3D11DeviceContext::RSGetScissorRects: Not implemented");
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
  
}
