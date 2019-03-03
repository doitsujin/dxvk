#pragma once

#include "../dxvk/dxvk_device.h"

#include "d3d9_device_child.h"

namespace dxvk {

  /**
   * \brief Buffer map mode
   */
  enum D3D9_COMMON_BUFFER_MAP_MODE {
    D3D9_COMMON_BUFFER_MAP_MODE_BUFFER,
    D3D9_COMMON_BUFFER_MAP_MODE_DIRECT
  };

  /**
   * \brief Common buffer descriptor
   */
  struct D3D9_BUFFER_DESC {
    D3DRESOURCETYPE Type;
    UINT Size;
    DWORD Usage;
    D3D9Format Format;
    D3DPOOL Pool;
    DWORD FVF;
  };

  /**
   * \brief The type of buffer you want to use
   */
  enum D3D9_COMMON_BUFFER_TYPE {
    D3D9_COMMON_BUFFER_TYPE_MAPPING,
    D3D9_COMMON_BUFFER_TYPE_STAGING,
    D3D9_COMMON_BUFFER_TYPE_REAL
  };

  class Direct3DCommonBuffer9 : public RcObject {
    static constexpr VkDeviceSize BufferSliceAlignment = 64;
  public:

    Direct3DCommonBuffer9(
            Direct3DDevice9Ex* pDevice,
      const D3D9_BUFFER_DESC*  pDesc);

    HRESULT Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            void** ppbData,
            DWORD  Flags);

    HRESULT Unlock();

    void GetDesc(
            D3D9_BUFFER_DESC* pDesc);

    D3D9_COMMON_BUFFER_MAP_MODE GetMapMode() const {
      return (m_buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        ? D3D9_COMMON_BUFFER_MAP_MODE_DIRECT
        : D3D9_COMMON_BUFFER_MAP_MODE_BUFFER;
    }

    Rc<DxvkBuffer> GetBuffer(D3D9_COMMON_BUFFER_TYPE type) const {
      if (type == D3D9_COMMON_BUFFER_TYPE_MAPPING)
        return GetMapBuffer();
      else if (type == D3D9_COMMON_BUFFER_TYPE_STAGING)
        return GetStagingBuffer();
      else //if (type == D3D9_COMMON_BUFFER_TYPE_REAL)
        return GetRealBuffer();
    }

    DxvkBufferSlice GetBufferSlice(D3D9_COMMON_BUFFER_TYPE type) const {
      return GetBufferSlice(type, 0, m_desc.Size);
    }

    DxvkBufferSlice GetBufferSlice(D3D9_COMMON_BUFFER_TYPE type, VkDeviceSize offset) const {
      return GetBufferSlice(type, offset, m_desc.Size - offset);
    }

    DxvkBufferSlice GetBufferSlice(D3D9_COMMON_BUFFER_TYPE type, VkDeviceSize offset, VkDeviceSize length) const {
      return DxvkBufferSlice(GetBuffer(type), offset, length);
    }

    DxvkBufferSliceHandle AllocMapSlice() {
      return GetMapBuffer()->allocSlice();
    }

    DxvkBufferSliceHandle DiscardMapSlice() {
      m_mapped = GetMapBuffer()->allocSlice();
      return m_mapped;
    }

    DxvkBufferSliceHandle GetMappedSlice() const {
      return m_mapped;
    }

    DWORD SetMapFlags(DWORD Flags) {
      DWORD old = m_mapFlags;
      m_mapFlags = Flags;
      return old;
    }

  private:

    Rc<DxvkBuffer> GetMapBuffer() const {
      return m_stagingBuffer != nullptr ? m_stagingBuffer : m_buffer;
    }

    Rc<DxvkBuffer> GetStagingBuffer() const {
      return m_stagingBuffer;
    }

    Rc<DxvkBuffer> GetRealBuffer() const {
      return m_buffer;
    }

    Direct3DDevice9Ex*          m_parent;
    const D3D9_BUFFER_DESC      m_desc;
    DWORD                       m_mapFlags;

    Rc<DxvkBuffer>              m_buffer;

    Rc<DxvkBuffer>              m_stagingBuffer;
    DxvkBufferSliceHandle       m_mapped;

  };

}