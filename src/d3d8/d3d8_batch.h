#pragma once

#include "d3d8_include.h"
#include "d3d8_buffer.h"
#include "d3d8_format.h"
#include "../d3d9/d3d9_bridge.h"

#include <vector>

namespace dxvk {
  
  /**
   * Vertex buffer that can handle many tiny locks while
   * still maintaing the lock ordering of direct-mapped buffers.
   */
  class D3D8BatchBuffer final : public D3D8VertexBuffer {
  public:
    // TODO: Don't need pBuffer, should avoid allocating it
    D3D8BatchBuffer(
        D3D8DeviceEx*                       pDevice,
        Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer,
        D3DPOOL                             Pool,
        DWORD                               Usage,
        UINT                                Length,
        DWORD                               FVF)
      : D3D8VertexBuffer(pDevice, std::move(pBuffer), Pool, Usage)
      , m_fvf(FVF)
      , m_stride(GetFVFStride(m_fvf))
      , m_data(Length) {
    }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            BYTE** ppbData,
            DWORD  Flags) {

      *ppbData = m_data.data() + OffsetToLock;
      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE Unlock() {
      return D3D_OK;
    }

    void STDMETHODCALLTYPE PreLoad() {
    }

    const void* GetPtr(UINT byteOffset = 0) const {
      return m_data.data() + byteOffset;
    }

    UINT Size() const {
      return m_data.size();
    }

  private:
    DWORD m_fvf = 0;
    UINT  m_stride = 0;

    std::vector<BYTE> m_data;
  };

  /**
   * Main handler for batching D3D8 draw calls.
   */
  class D3D8Batcher {

    struct Batch {
      UINT StartVertex;
      UINT PrimitiveCount;
      UINT VertexCount;
      UINT DrawCallCount = 1;
    };

  public:
    D3D8Batcher(D3D9Bridge* bridge, Com<d3d9::IDirect3DDevice9>&& pDevice)
      : m_bridge(bridge)
      , m_device(std::move(pDevice)) {}

    inline void StateChange() {
      if (m_batches.empty())
        return;
      for (auto& draw : m_batches) {
        m_bridge->AddBatchCalls(draw.DrawCallCount);

        m_device->DrawPrimitiveUP(
          d3d9::D3DPRIMITIVETYPE(m_primitiveType),
          draw.PrimitiveCount,
          m_stream->GetPtr(draw.StartVertex * m_stride),
          m_stride);

      }
      m_device->SetStreamSource(0, D3D8VertexBuffer::GetD3D9Nullable(m_stream), 0, m_stride);
      m_batches.clear();
    }

    inline void EndFrame() {
    }

    inline HRESULT DrawPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             StartVertex,
            UINT             PrimitiveCount) {

      if (m_primitiveType != PrimitiveType) {
        StateChange();
        m_primitiveType = PrimitiveType;
      }

      m_batches.push_back({
        StartVertex,
        PrimitiveCount,
        GetVertexCount8(PrimitiveType, PrimitiveCount)
      });
      return D3D_OK;
    }

    inline void SetStream(UINT num, D3D8VertexBuffer* stream, UINT stride) {
      if (unlikely(num != 0)) {
        StateChange();
        return;
      }
      if (unlikely(m_stream != stream || m_stride != stride)) {
        StateChange();
        // TODO: Not optimal
        m_stream = static_cast<D3D8BatchBuffer*>(stream);
        m_stride = stride;
      }
    }

  private:
    D3D9Bridge*                   m_bridge;
    Com<d3d9::IDirect3DDevice9>   m_device;

    D3D8BatchBuffer*              m_stream = nullptr;
    UINT                          m_stride = 0;
    std::vector<Batch>            m_batches;
    D3DPRIMITIVETYPE              m_primitiveType = D3DPRIMITIVETYPE(0);
  };
}
