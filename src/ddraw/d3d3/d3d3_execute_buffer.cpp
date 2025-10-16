#include "d3d3_execute_buffer.h"

#include "../d3d_multithread.h"

namespace dxvk {

  std::atomic<uint32_t> D3D3ExecuteBuffer::s_buffCount = 0;

  D3D3ExecuteBuffer::D3D3ExecuteBuffer(D3D3Device* pParent, D3DEXECUTEBUFFERDESC* pDesc)
    : DDrawChildObject<D3D3Device, IDirect3DExecuteBuffer>(pParent) {
    if (likely(pDesc->dwFlags & D3DDEB_BUFSIZE)) {
      m_buffer.resize(pDesc->dwBufferSize);
      Logger::debug(str::format("D3D3ExecuteBuffer: Buffer is initialized with size: ", pDesc->dwBufferSize));
    } else {
      Logger::warn("D3D3ExecuteBuffer: No buffer size specified during initialization");
    }

    m_buffCount = ++s_buffCount;

    Logger::debug(str::format("D3D3ExecuteBuffer: Created a new execute buffer nr. {{1-", m_buffCount, "}}:"));
  }

  D3D3ExecuteBuffer::~D3D3ExecuteBuffer() {
    Logger::debug(str::format("D3D3ExecuteBuffer: Execute buffer nr. {{1-", m_buffCount, "}} bites the dust"));
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

    if (unlikely(m_buffer.size() == 0))
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

    if (unlikely(m_locked))
      return D3DERR_EXECUTE_LOCKED;

    if (unlikely(lpDesc->dwSize != sizeof(D3DEXECUTEBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

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

    if (unlikely(lpData == nullptr || m_buffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwInstructionOffset + lpData->dwInstructionLength > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwVertexOffset + lpData->dwVertexCount > m_buffer.size()))
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
