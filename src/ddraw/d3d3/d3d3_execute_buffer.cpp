#include "d3d3_execute_buffer.h"

#include "../d3d_multithread.h"

namespace dxvk {

  D3D3ExecuteBuffer::D3D3ExecuteBuffer(D3D3Device* pParent, D3DEXECUTEBUFFERDESC* pDesc)
    : DDrawChildObject<D3D3Device, IDirect3DExecuteBuffer>(pParent) {
    m_buffer.resize(pDesc->dwBufferSize);
  }

  D3D3ExecuteBuffer::~D3D3ExecuteBuffer() {
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DExecuteBuffer))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3ExecuteBuffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::GetExecuteData(LPD3DEXECUTEDATA lpData) {
    D3DDeviceLock lock;

    if (unlikely(m_executed))
      lock = m_parent->LockDevice();

    if (unlikely(lpData == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpData = m_executeData;

    return D3D_OK;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DExecuteBuffer object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Lock(LPD3DEXECUTEBUFFERDESC lpDesc) {
    D3DDeviceLock lock;

    if (unlikely(m_executed))
      lock = m_parent->LockDevice();

    if (unlikely(lpDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDesc->dwSize != sizeof(D3DEXECUTEBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_locked))
      return D3DERR_EXECUTE_LOCKED;

    lpDesc->dwFlags = D3DDEB_BUFSIZE | D3DDEB_LPDATA;
    lpDesc->dwBufferSize = m_buffer.size();
    lpDesc->lpData = m_buffer.data();

    m_locked = true;

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Optimize(DWORD dwUnknown) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::SetExecuteData(LPD3DEXECUTEDATA lpData) {
    D3DDeviceLock lock;

    if (unlikely(m_executed))
      lock = m_parent->LockDevice();

    if (unlikely(lpData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwSize != sizeof(D3DEXECUTEDATA)))
      return DDERR_INVALIDPARAMS;

    // The checks below will protect against a m_buffer.size() of 0
    if (unlikely(lpData->dwInstructionOffset + lpData->dwInstructionLength > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwVertexOffset + lpData->dwVertexCount * sizeof(D3DVERTEX) > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwHVertexOffset + lpData->dwVertexCount * sizeof(D3DTLVERTEX) > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    m_executeData = *lpData;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Unlock() {
    D3DDeviceLock lock;

    if (unlikely(m_executed))
      lock = m_parent->LockDevice();

    if (unlikely(!m_locked))
      return D3DERR_EXECUTE_NOT_LOCKED;

    m_locked = false;

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved) {
    return DDERR_UNSUPPORTED;
  }

}
