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
    UINT            Size;
    DWORD           Usage;
    D3D9Format      Format;
    D3DPOOL         Pool;
    DWORD           FVF;
  };

  /**
   * \brief The type of buffer you want to use
   */
  enum D3D9_COMMON_BUFFER_TYPE {
    D3D9_COMMON_BUFFER_TYPE_MAPPING,
    D3D9_COMMON_BUFFER_TYPE_STAGING,
    D3D9_COMMON_BUFFER_TYPE_REAL
  };

  class D3D9CommonBuffer {
    static constexpr VkDeviceSize BufferSliceAlignment = 64;
  public:

    D3D9CommonBuffer(
            D3D9DeviceEx*      pDevice,
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
      return m_desc.Usage & D3DUSAGE_DYNAMIC
        ? D3D9_COMMON_BUFFER_MAP_MODE_DIRECT
        : D3D9_COMMON_BUFFER_MAP_MODE_BUFFER;
    }

    template <D3D9_COMMON_BUFFER_TYPE Type>
    Rc<DxvkBuffer> GetBuffer() const {
      if constexpr (Type == D3D9_COMMON_BUFFER_TYPE_MAPPING)
        return GetMapBuffer();
      else if constexpr (Type == D3D9_COMMON_BUFFER_TYPE_STAGING)
        return GetStagingBuffer();
      else //if constexpr (Type == D3D9_COMMON_BUFFER_TYPE_REAL)
        return GetRealBuffer();
    }

    template <D3D9_COMMON_BUFFER_TYPE Type>
    DxvkBufferSlice GetBufferSlice() const {
      return GetBufferSlice<Type>(0, m_desc.Size);
    }

    template <D3D9_COMMON_BUFFER_TYPE Type>
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset) const {
      return GetBufferSlice<Type>(offset, m_desc.Size - offset);
    }

    template <D3D9_COMMON_BUFFER_TYPE Type>
    DxvkBufferSlice GetBufferSlice(VkDeviceSize offset, VkDeviceSize length) const {
      return DxvkBufferSlice(GetBuffer<Type>(), offset, length);
    }

    DxvkBufferSliceHandle AllocMapSlice() {
      return GetMapBuffer()->allocSlice();
    }

    DxvkBufferSliceHandle DiscardMapSlice() {
      m_sliceHandle = GetMapBuffer()->allocSlice();
      return m_sliceHandle;
    }

    DxvkBufferSliceHandle GetMappedSlice() const {
      return m_sliceHandle;
    }

    DWORD SetMapFlags(DWORD Flags) {
      DWORD old = m_mapFlags;
      m_mapFlags = Flags;
      return old;
    }

    const D3D9_BUFFER_DESC* Desc() const {
      return &m_desc;
    }

  private:

    Rc<DxvkBuffer> CreateBuffer() const;
    Rc<DxvkBuffer> CreateStagingBuffer() const;

    Rc<DxvkBuffer> GetMapBuffer() const {
      return m_stagingBuffer != nullptr ? m_stagingBuffer : m_buffer;
    }

    Rc<DxvkBuffer> GetStagingBuffer() const {
      return m_stagingBuffer;
    }

    Rc<DxvkBuffer> GetRealBuffer() const {
      return m_buffer;
    }

    D3D9DeviceEx*               m_parent;
    const D3D9_BUFFER_DESC      m_desc;
    DWORD                       m_mapFlags;

    Rc<DxvkBuffer>              m_buffer;
    Rc<DxvkBuffer>              m_stagingBuffer;

    DxvkBufferSliceHandle       m_sliceHandle;

  };

}