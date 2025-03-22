#pragma once

#include "d3d8_include.h"
#include "d3d8_buffer.h"
#include "d3d8_format.h"

#include <vector>
#include <cstdint>

namespace dxvk {

  inline constexpr size_t            D3DPT_COUNT   = size_t(D3DPT_TRIANGLEFAN) + 1;
  inline constexpr D3DPRIMITIVETYPE  D3DPT_INVALID = D3DPRIMITIVETYPE(0);

  // Vertex buffer that can handle many tiny locks while
  // still maintaing the lock ordering of direct-mapped buffers.
  class D3D8BatchBuffer final : public D3D8VertexBuffer {

  public:

    D3D8BatchBuffer(
        D3D8Device*                         pDevice,
        D3DPOOL                             Pool,
        DWORD                               Usage,
        UINT                                Length,
        DWORD                               FVF)
      : D3D8VertexBuffer(pDevice, nullptr, Pool, Usage)
      , m_data(Length)
      , m_fvf(FVF) {
    }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            BYTE** ppbData,
            DWORD  Flags) final {
      *ppbData = m_data.data() + OffsetToLock;
      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE Unlock() final {
      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc) final {
      if (unlikely(pDesc == nullptr))
        return D3DERR_INVALIDCALL;

      pDesc->Format = D3DFMT_VERTEXDATA;
      pDesc->Type   = D3DRTYPE_VERTEXBUFFER;
      pDesc->Usage  = m_usage;
      pDesc->Pool   = m_pool;
      pDesc->Size   = m_data.size();
      pDesc->FVF    = m_fvf;

      return D3D_OK;
    }

    void STDMETHODCALLTYPE PreLoad() final {
    }

    const void* GetPtr(UINT byteOffset = 0) const {
      return m_data.data() + byteOffset;
    }

    UINT Size() const {
      return m_data.size();
    }

  private:

    std::vector<BYTE> m_data;
    DWORD             m_fvf;

  };


  // Main handler for batching D3D8 draw calls.
  class D3D8Batcher {

    struct Batch {
      D3DPRIMITIVETYPE PrimitiveType = D3DPT_INVALID;
      std::vector<uint16_t> Indices;
      UINT Offset = 0;
      UINT MinVertex = std::numeric_limits<uint32_t>::max();
      UINT MaxVertex = 0;
      UINT PrimitiveCount = 0;
      UINT DrawCallCount = 0;
    };

  public:

    D3D8Batcher(D3D8Device* pDevice8, Com<d3d9::IDirect3DDevice9>&& pDevice9)
      : m_device8(pDevice8)
      , m_device(std::move(pDevice9)) {
    }

    inline D3D8BatchBuffer* CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool) {
      return ref(new D3D8BatchBuffer(m_device8, Pool, Usage, Length, FVF));
    }

    inline void StateChange() {
      if (likely(m_batches.empty()))
        return;
      for (auto& draw : m_batches) {

        if (draw.PrimitiveType == D3DPT_INVALID)
          continue;

        for (auto& index : draw.Indices)
          index -= draw.MinVertex;

        m_device->DrawIndexedPrimitiveUP(
          d3d9::D3DPRIMITIVETYPE(draw.PrimitiveType),
          0,
          draw.MaxVertex - draw.MinVertex,
          draw.PrimitiveCount,
          draw.Indices.data(),
          d3d9::D3DFMT_INDEX16,
          m_stream->GetPtr(draw.MinVertex * m_stride),
          m_stride);

        m_device->SetStreamSource(0, D3D8VertexBuffer::GetD3D9Nullable(m_stream), 0, m_stride);
        m_device->SetIndices(D3D8IndexBuffer::GetD3D9Nullable(m_indices));

        draw.PrimitiveType = D3DPRIMITIVETYPE(0);
        draw.Offset = 0;
        draw.MinVertex = std::numeric_limits<uint32_t>::max();
        draw.MaxVertex = 0;
        draw.PrimitiveCount = 0;
        draw.DrawCallCount = 0;
      }
    }

    inline void EndFrame() {
      // Nothing to be done.
    }

    inline HRESULT DrawPrimitive(
            D3DPRIMITIVETYPE PrimitiveType,
            UINT             StartVertex,
            UINT             PrimitiveCount) {

      // None of this linestrip or fan malarkey
      D3DPRIMITIVETYPE batchedPrimType = PrimitiveType;
      switch (PrimitiveType) {
        case D3DPT_LINESTRIP:     batchedPrimType = D3DPT_LINELIST; break;
        case D3DPT_TRIANGLEFAN:   batchedPrimType = D3DPT_TRIANGLELIST; break;
        default: break;
      }

      Batch* batch = &m_batches[size_t(batchedPrimType)];
      batch->PrimitiveType = batchedPrimType;

      //UINT vertices = GetVertexCount8(PrimitiveType, PrimitiveCount);

      switch (PrimitiveType) {
        case D3DPT_POINTLIST:
          batch->Indices.resize(batch->Offset + PrimitiveCount);
          for (UINT i = 0; i < PrimitiveCount; i++)
            batch->Indices[batch->Offset++] = (StartVertex + i);
          break;
        case D3DPT_LINELIST:
          batch->Indices.resize(batch->Offset + PrimitiveCount * 2);
          for (UINT i = 0; i < PrimitiveCount; i++) {
            batch->Indices[batch->Offset++] = (StartVertex + i * 2 + 0);
            batch->Indices[batch->Offset++] = (StartVertex + i * 2 + 1);
          }
          break;
        case D3DPT_LINESTRIP:
          batch->Indices.resize(batch->Offset + PrimitiveCount * 2);
          for (UINT i = 0; i < PrimitiveCount; i++) {
            batch->Indices[batch->Offset++] = (StartVertex + i + 0);
            batch->Indices[batch->Offset++] = (StartVertex + i + 1);
          }
          break;
        case D3DPT_TRIANGLELIST:
          batch->Indices.resize(batch->Offset + PrimitiveCount * 3);
          for (UINT i = 0; i < PrimitiveCount; i++) {
            batch->Indices[batch->Offset++] = (StartVertex + i * 3 + 0);
            batch->Indices[batch->Offset++] = (StartVertex + i * 3 + 1);
            batch->Indices[batch->Offset++] = (StartVertex + i * 3 + 2);
          }
          break;
        case D3DPT_TRIANGLESTRIP:
          // Join with degenerate triangle
          // 1 2 3, 3 4, 4 5 6
          batch->Indices.resize(batch->Offset + PrimitiveCount + 2);
          if (batch->Offset > 0) {
            batch->Indices[batch->Offset + 1] = batch->Indices[batch->Offset-2];
            batch->Indices[batch->Offset += 2] = StartVertex;
          }
          for (UINT i = 0; i < PrimitiveCount; i++) {
            batch->Indices[batch->Offset++] = (StartVertex + i + 0);
          }
          break;
        // 1 2 3 4 5 6 7 -> 1 2 3, 1 3 4, 1 4 5, 1 5 6, 1 6 7
        case D3DPT_TRIANGLEFAN:
          batch->Indices.resize(batch->Offset + PrimitiveCount * 3);
          for (UINT i = 0; i < PrimitiveCount; i++) {
            batch->Indices[batch->Offset++] = (StartVertex + 0);
            batch->Indices[batch->Offset++] = (StartVertex + i + 1);
            batch->Indices[batch->Offset++] = (StartVertex + i + 2);
          }
          break;
        default:
          return D3DERR_INVALIDCALL;
      }
      batch->MinVertex = std::min(batch->MinVertex, StartVertex);
      if (!batch->Indices.empty())
        batch->MaxVertex = std::max(batch->MaxVertex, UINT(batch->Indices.back() + 1));
      batch->PrimitiveCount += PrimitiveCount;
      batch->DrawCallCount++;
      return D3D_OK;
    }

    inline void SetStream(UINT num, D3D8VertexBuffer* stream, UINT stride) {
      if (unlikely(num != 0)) {
        StateChange();
        return;
      }
      if (unlikely(m_stream != stream || m_stride != stride)) {
        StateChange();
        m_stream = static_cast<D3D8BatchBuffer*>(stream);
        m_stride = stride;
      }
    }

    inline void SetIndices(D3D8IndexBuffer* indices, INT baseVertexIndex) {
      if (m_indices != indices || m_baseVertexIndex != baseVertexIndex) {
        StateChange();
        m_indices = indices;
        m_baseVertexIndex = baseVertexIndex;
      }
    }

  private:

    D3D8Device*                     m_device8;
    Com<d3d9::IDirect3DDevice9>     m_device;

    D3D8BatchBuffer*                m_stream = nullptr;
    UINT                            m_stride = 0;
    D3D8IndexBuffer*                m_indices = nullptr;
    INT                             m_baseVertexIndex = 0;
    std::array<Batch, D3DPT_COUNT>  m_batches;

  };

}
