#include "d3d9_common_buffer.h"

#include "d3d9_util.h"

namespace dxvk {

  Direct3DCommonBuffer9::Direct3DCommonBuffer9(
          Direct3DDevice9Ex* pDevice,
    const D3D9_BUFFER_DESC*  pDesc) 
    : m_parent { pDevice }
    , m_desc   { *pDesc } {
    DxvkBufferCreateInfo  info;
    info.size   = m_desc.Size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;

    if (pDesc->Type == D3DRTYPE_VERTEXBUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    else if (pDesc->Type == D3DRTYPE_INDEXBUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }

    if (pDesc->Usage & D3DUSAGE_DYNAMIC) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;

      if (!(pDesc->Usage & D3DUSAGE_WRITEONLY))
        info.access |= VK_ACCESS_HOST_READ_BIT;
    }

    VkMemoryPropertyFlags memoryFlags = GetMemoryFlagsForUsage(pDesc->Usage);

    // Create the buffer and set the entire buffer slice as mapped,
    // so that we only have to update it when invalidating the buffer
    m_buffer = m_parent->GetDXVKDevice()->createBuffer(info, memoryFlags);
    m_mapped = m_buffer->getSliceHandle();

    if (GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;

      memoryFlags  = GetMemoryFlagsForUsage(pDesc->Usage | D3DUSAGE_DYNAMIC);
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

      m_stagingBuffer = m_parent->GetDXVKDevice()->createBuffer(info, memoryFlags);
      m_mapped = m_stagingBuffer->getSliceHandle();
    }
  }

  HRESULT Direct3DCommonBuffer9::Lock(
          UINT   OffsetToLock,
          UINT   SizeToLock,
          void** ppbData,
          DWORD  Flags) {
    return m_parent->LockBuffer(
      this,
      OffsetToLock,
      SizeToLock,
      ppbData,
      Flags);
  }

  HRESULT Direct3DCommonBuffer9::Unlock() {
    return m_parent->UnlockBuffer(this);
  }

  void Direct3DCommonBuffer9::GetDesc(
          D3D9_BUFFER_DESC* pDesc) {
    *pDesc = m_desc;
  }

}