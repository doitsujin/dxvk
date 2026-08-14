#include "d3d7_buffer.h"

#include "../d3d_common_device.h"

#include "../ddraw_util.h"

#include "../d3d_process_vertices.h"
#include "../d3d_multithread.h"

#include "../ddraw7/ddraw7_interface.h"

#include <vector>

namespace dxvk {

  D3D7VertexBuffer::D3D7VertexBuffer(
        D3D7Interface* pParent,
        D3DVERTEXBUFFERDESC* pDesc)
    : DDrawChildObject<D3D7Interface, IDirect3DVertexBuffer7>(pParent)
    , m_commonIntf ( pParent->GetCommonInterface() )
    , m_desc ( *pDesc )
    , m_stride ( GetFVFSize(pDesc->dwFVF) )
    , m_size ( m_stride * pDesc->dwNumVertices ) {
    m_parent->AddRef();

    // In the fortunate scenario where a D3D7 device is already present
    // when a vertex buffer is created, initialize the buffer on the spot
    // rather than deferring the initialization to the first Lock()
    // or ProcessVertices() call, since that can cause hitching
    RefreshD3DDevice();
    if (m_d3d7Device != nullptr)
      InitializeD3D9();
  }

  D3D7VertexBuffer::~D3D7VertexBuffer() {
    m_parent->Release();
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DVertexBuffer7))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D7VertexBuffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    const DWORD dwSize = lpVBDesc->dwSize;

    *lpVBDesc = m_desc;
    // The value passed in dwSize during the query is expected to be
    // preserved, even if it is not equal to sizeof(D3DVERTEXBUFFERDESC)
    lpVBDesc->dwSize = dwSize;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
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

    // Cops 2170: The Power of Law relies on us not discarding on any write only lock
    // to render geometry, and does not mark the affected buffers with D3DVBCAPS_WRITEONLY
    const bool legacyDiscard = m_legacyDiscard | (m_commonIntf->GetOptions()->forceLegacyDiscard
                                                  && (flags & DDLOCK_WRITEONLY));

    HRESULT hr = m_vb9->Lock(0, 0, data, ConvertD3D7LockFlags(flags, legacyDiscard, false));
    if (unlikely(FAILED(hr)))
      return hr;

    m_locked = true;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Unlock() {
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

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    if (unlikely(!dwCount))
      return D3D_OK;

    if (unlikely(lpD3DDevice == nullptr || lpSrcBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!(dwVertexOp & D3DVOP_TRANSFORM)))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device7 = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7VertexBuffer* srcBuffer7 = static_cast<D3D7VertexBuffer*>(lpSrcBuffer);

    // Check and initialize the source buffer
    srcBuffer7->RefreshD3DDevice();
    if (unlikely(!srcBuffer7->IsInitialized())) {
      HRESULT hrInit = srcBuffer7->InitializeD3D9();
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

    if (unlikely(m_d3d7Device != device7)) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Invalid device");
      return DDERR_GENERIC;
    }

    D3DDeviceLock lock = device7->LockDevice();

    d3d9::IDirect3DDevice9* device9 = device7->GetCommonD3DDevice()->GetD3D9Device();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (likely(d3dOptions->cpuProcessVertices)) {
      uint8_t *inData = nullptr;
      uint8_t *outData = nullptr;

      d3d9::IDirect3DVertexBuffer9* srcBuffer9 = srcBuffer7->GetD3D9VertexBuffer();

      HRESULT hr = srcBuffer9->Lock(dwSrcIndex * srcBuffer7->GetStride(), dwCount * srcBuffer7->GetStride(),
                                    reinterpret_cast<void**>(&inData), D3DLOCK_READONLY);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed to lock source buffer");
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      hr = m_vb9->Lock(dwDestIndex * m_stride, dwCount * m_stride, reinterpret_cast<void**>(&outData), 0);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed to lock destination buffer");
        srcBuffer9->Unlock();
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      const bool doLighting = dwVertexOp & D3DVOP_LIGHT;

      ProcessVerticesData pvData;
      pvData.inData = inData;
      pvData.inFVF = srcBuffer7->GetFVF();
      pvData.inStride = srcBuffer7->GetStride();
      pvData.outData = outData;
      pvData.outFVF = m_desc.dwFVF;
      pvData.outStride = m_stride;
      pvData.vertexCount = dwCount;
      pvData.correction = nullptr;
      pvData.dsStatus = nullptr;
      pvData.doLighting = doLighting;
      pvData.doClipping = dwVertexOp & D3DVOP_CLIP;
      pvData.doNotCopyData = dwFlags & D3DPV_DONOTCOPYDATA;
      pvData.doExtents = true;
      pvData.isLegacy = false;

      std::vector<d3d9::D3DLIGHT9> lights9;
      if (doLighting) {
        device7->GetD3D9ActiveLights(&lights9);
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
        Logger::warn("D3D7VertexBuffer::ProcessVertices: Unsupported operation D3DVOP_LIGHT");

      device9->SetFVF(srcBuffer7->GetFVF());
      device9->SetStreamSource(0, srcBuffer7->GetD3D9VertexBuffer(), 0, srcBuffer7->GetStride());
      HRESULT hr = device9->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_vb9.ptr(), nullptr, dwFlags);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
        return hr;
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::warn("!!! D3D7VertexBuffer::ProcessVerticesStrided: Stub");

    if (unlikely(!dwCount))
      return D3D_OK;

    if (unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device7 = static_cast<D3D7Device*>(lpD3DDevice);

    // Check and initialize the destination buffer (this buffer)
    RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    if (unlikely(m_d3d7Device != device7)) {
      Logger::err("D3D7VertexBuffer::ProcessVerticesStrided: Invalid device");
      return DDERR_GENERIC;
    }

    D3DDeviceLock lock = device7->LockDevice();

    //d3d9::IDirect3DDevice9* device9 = device7->GetCommonD3DDevice()->GetD3D9Device();

    // TODO: lpVertexArray needs to be transformed into a non-strided vertex buffer stream

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    if (unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(IsLocked()))
      return D3DERR_VERTEXBUFFERLOCKED;

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    m_desc.dwCaps |= D3DVBCAPS_OPTIMIZED;

    return D3D_OK;
  };

  HRESULT D3D7VertexBuffer::InitializeD3D9() {
    // Can't create anything without a valid device
    if (unlikely(m_d3d7Device == nullptr)) {
      Logger::warn("D3D7VertexBuffer::InitializeD3D9: Null D3D7 device, can't initialize right now");
      return DDERR_GENERIC;
    }

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    const d3d9::D3DPOOL pool = (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY) ? d3d9::D3DPOOL_SYSTEMMEM :
                               d3dOptions->managedVertexBuffers ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_DEFAULT;
    const DWORD usage = ConvertD3D7UsageFlags(m_desc.dwCaps, pool);
    m_legacyDiscard = m_commonIntf->GetOptions()->forceLegacyDiscard &&
                      (usage & D3DUSAGE_DYNAMIC) && (usage & D3DUSAGE_WRITEONLY);

    d3d9::IDirect3DDevice9* device9 = m_d3d7Device->GetCommonD3DDevice()->GetD3D9Device();
    HRESULT hr = device9->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_vb9, nullptr);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::InitializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    return D3D_OK;
  }

  void D3D7VertexBuffer::RefreshD3DDevice() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    D3D7Device* d3d7Device = commonD3DDevice != nullptr ? commonD3DDevice->GetD3D7Device() : nullptr;
    if (unlikely(m_d3d7Device != d3d7Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (unlikely(m_d3d7Device != nullptr)) {
        Logger::debug("D3D7VertexBuffer::RefreshD3DDevice: Device context has changed, clearing D3D9 buffers");
        m_vb9 = nullptr;
      }
      m_d3d7Device = d3d7Device;
    }
  }

}
