#include "d3d6_buffer.h"

#include "../d3d_common_device.h"

#include "../ddraw_util.h"

#include "../d3d_process_vertices.h"
#include "../d3d_multithread.h"

#include "../ddraw4/ddraw4_interface.h"

#include <vector>

namespace dxvk {

  D3D6VertexBuffer::D3D6VertexBuffer(
        D3D6Interface* pParent,
        DWORD creationFlags,
        D3DVERTEXBUFFERDESC* pDesc)
    : DDrawChildObject<D3D6Interface, IDirect3DVertexBuffer>(pParent)
    , m_commonIntf ( pParent->GetCommonInterface() )
    , m_creationFlags ( creationFlags )
    , m_desc ( *pDesc )
    , m_stride ( GetFVFSize(pDesc->dwFVF) )
    , m_size ( m_stride * pDesc->dwNumVertices ) {
    // In the fortunate scenario where a D3D6 device is already present
    // when a vertex buffer is created, initialize the buffer on the spot
    // rather than deferring the initialization to the first Lock()
    // or ProcessVertices() call, since that can cause hitching
    RefreshD3DDevice();
    if (m_d3d6Device != nullptr)
      InitializeD3D9();
  }

  D3D6VertexBuffer::~D3D6VertexBuffer() {
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DVertexBuffer))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D6VertexBuffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    const DWORD dwSize = lpVBDesc->dwSize;

    *lpVBDesc = m_desc;
    // The value passed in dwSize during the query is expected to be
    // preserved, even if it is not equal to sizeof(D3DVERTEXBUFFERDESC)
    lpVBDesc->dwSize = dwSize;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    if (data_size != nullptr)
      *data_size = m_size;

    HRESULT hr = m_vb9->Lock(0, 0, data, ConvertD3D6LockFlags(flags, false));
    if (unlikely(FAILED(hr)))
      return hr;

    m_locked = true;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Unlock() {
    // Ignore the unlock call if the D3D9 buffer
    // was lost since the previous Lock() call
    if (unlikely(!IsInitialized()))
      return D3D_OK;

    HRESULT hr = m_vb9->Unlock();
    if (unlikely(FAILED(hr)))
      return D3DERR_VERTEXBUFFERUNLOCKFAILED;

    m_locked = false;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags) {
    if (unlikely(!dwCount))
      return D3D_OK;

    if (unlikely(lpD3DDevice == nullptr || lpSrcBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!(dwVertexOp & D3DVOP_TRANSFORM)))
      return DDERR_INVALIDPARAMS;

    D3D6Device* device6 = static_cast<D3D6Device*>(lpD3DDevice);
    D3D6VertexBuffer* srcBuffer6 = static_cast<D3D6VertexBuffer*>(lpSrcBuffer);

    // Check and initialize the source buffer
    srcBuffer6->RefreshD3DDevice();
    if (unlikely(!srcBuffer6->IsInitialized())) {
      HRESULT hrInit = srcBuffer6->InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    // Check and initialize the destination buffer (this buffer)
    RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    if (unlikely(m_d3d6Device != device6)) {
      Logger::err("D3D6VertexBuffer::ProcessVertices: Invalid device");
      return DDERR_GENERIC;
    }

    D3DDeviceLock lock = device6->LockDevice();

    d3d9::IDirect3DDevice9* device9 = device6->GetCommonD3DDevice()->GetD3D9Device();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (likely(d3dOptions->cpuProcessVertices)) {
      uint8_t *inData = nullptr;
      uint8_t *outData = nullptr;

      d3d9::IDirect3DVertexBuffer9* srcBuffer9 = srcBuffer6->GetD3D9VertexBuffer();

      HRESULT hr = srcBuffer9->Lock(dwSrcIndex * srcBuffer6->GetStride(), dwCount * srcBuffer6->GetStride(),
                                    reinterpret_cast<void**>(&inData), D3DLOCK_READONLY);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed to lock source buffer");
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      hr = m_vb9->Lock(dwDestIndex * m_stride, dwCount * m_stride, reinterpret_cast<void**>(&outData), 0);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed to lock destination buffer");
        srcBuffer9->Unlock();
        return D3DERR_VERTEXBUFFERLOCKED;
      }
      // "Direct3D normally performs lighting calculations on any vertices that contain a vertex normal."
      // "If your application uses vertex buffers, include or omit the D3DVOP_LIGHT flag when calling the
      //  IDirect3DVertexBuffer::ProcessVertices method to enable or disable lighting for that vertex buffer."
      // "If the rendering device does not have a material assigned to it, the Direct3D lighting engine is disabled."
      const bool doLighting = (dwVertexOp & D3DVOP_LIGHT) &&
                              (srcBuffer6->GetFVF() & D3DFVF_NORMAL) &&
                              device6->GetCommonD3DDevice()->GetCurrentMaterialHandle() != 0;

      D3DCommonViewport* commonViewport = device6->GetCurrentViewportInternal()->GetCommonViewport();

      ProcessVerticesData pvData;
      pvData.inData = inData;
      pvData.inFVF = srcBuffer6->GetFVF();
      pvData.inStride = srcBuffer6->GetStride();
      pvData.outData = outData;
      pvData.outFVF = m_desc.dwFVF;
      pvData.outStride = m_stride;
      pvData.vertexCount = dwCount;
      pvData.correction = commonViewport->GetLegacyProjectionMatrix(0);
      pvData.dsStatus = nullptr;
      pvData.doLighting = doLighting;
      pvData.doClipping = dwVertexOp & D3DVOP_CLIP;
      pvData.doNotCopyData = dwFlags & D3DPV_DONOTCOPYDATA;
      pvData.doExtents = true;
      pvData.isLegacy = true;

      std::vector<d3d9::D3DLIGHT9> lights9;
      if (doLighting) {
        commonViewport->GetD3D9ActiveLights(&lights9);
        pvData.lights = &lights9;
      } else {
        pvData.lights = nullptr;
      }

      ProcessVerticesSW(device9, m_commonIntf->GetOptions(), &pvData);

      m_vb9->Unlock();
      srcBuffer9->Unlock();

    } else {
      // D3D9 ProcessVertices doesn't handle lighting, only transforms
      if (unlikely(dwVertexOp & D3DVOP_LIGHT))
        Logger::warn("D3D6VertexBuffer::ProcessVertices: Unsupported operation D3DVOP_LIGHT");

      D3DMATRIX projectionMatrix;
      const D3DMATRIX* legacyProjection = nullptr;

      D3DCommonViewport* commonViewport = device6->GetCurrentViewportInternal()->GetCommonViewport();
      if (likely(commonViewport != nullptr)) {
        legacyProjection = commonViewport->GetLegacyProjectionMatrix(0);

        if (legacyProjection != nullptr) {
          //Logger::debug("D3D6Device: Applying legacy projection");
          device9->GetTransform(d3d9::D3DTS_PROJECTION, &projectionMatrix);
          device9->MultiplyTransform(d3d9::D3DTS_PROJECTION, legacyProjection);
        }
      }

      device9->SetFVF(srcBuffer6->GetFVF());
      device9->SetStreamSource(0, srcBuffer6->GetD3D9VertexBuffer(), 0, srcBuffer6->GetStride());
      HRESULT hr = device9->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_vb9.ptr(), nullptr, dwFlags);

      if (legacyProjection != nullptr) {
        //Logger::debug("D3D6Device: Reverting legacy projection");
        device9->SetTransform(d3d9::D3DTS_PROJECTION, &projectionMatrix);
      }

      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
        return hr;
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags) {
    if (unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_locked))
      return D3DERR_VERTEXBUFFERLOCKED;

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    m_desc.dwCaps |= D3DVBCAPS_OPTIMIZED;

    return D3D_OK;
  };

  HRESULT D3D6VertexBuffer::InitializeD3D9() {
    // Can't create anything without a valid device
    if (unlikely(m_d3d6Device == nullptr)) {
      Logger::warn("D3D6VertexBuffer::InitializeD3D9: Null D3D6 device, can't initialize right now");
      return DDERR_GENERIC;
    }

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    const d3d9::D3DPOOL pool = (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY) ? d3d9::D3DPOOL_SYSTEMMEM :
                               d3dOptions->managedVertexBuffers ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_DEFAULT;
    const DWORD usage = ConvertD3D6UsageFlags(m_desc.dwCaps, m_creationFlags, pool);

    d3d9::IDirect3DDevice9* device9 = m_d3d6Device->GetCommonD3DDevice()->GetD3D9Device();
    HRESULT hr = device9->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_vb9, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6VertexBuffer::InitializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    return D3D_OK;
  }

  void D3D6VertexBuffer::RefreshD3DDevice() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    D3D6Device* d3d6Device = commonD3DDevice != nullptr ? commonD3DDevice->GetD3D6Device() : nullptr;
    if (unlikely(m_d3d6Device != d3d6Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (unlikely(m_d3d6Device != nullptr)) {
        Logger::debug("D3D6VertexBuffer::RefreshD3DDevice: Device context has changed, clearing D3D9 buffers");
        m_vb9 = nullptr;
      }
      m_d3d6Device = d3d6Device;
    }
  }

}
