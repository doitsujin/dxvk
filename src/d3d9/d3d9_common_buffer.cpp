#include "d3d9_common_buffer.h"

#include "d3d9_device.h"
#include "d3d9_util.h"

namespace dxvk {

  D3D9CommonBuffer::D3D9CommonBuffer(
          D3D9DeviceEx*      pDevice,
    const D3D9_BUFFER_DESC*  pDesc)
    : m_parent ( pDevice ), m_desc ( *pDesc ),
      m_mapMode(DetermineMapMode(pDevice->GetOptions())) {
    m_buffer = CreateBuffer();
    if (m_mapMode == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      m_stagingBuffer = CreateStagingBuffer();

    m_allocation = GetMapBuffer()->storage();

    if (m_desc.Pool != D3DPOOL_DEFAULT)
      m_dirtyRange = D3D9Range(0, m_desc.Size);
  }

  D3D9CommonBuffer::~D3D9CommonBuffer() {
    if (m_desc.Pool == D3DPOOL_DEFAULT)
      m_parent->DecrementLosableCounter();
  }


  HRESULT D3D9CommonBuffer::Lock(
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


  HRESULT D3D9CommonBuffer::Unlock() {
    return m_parent->UnlockBuffer(this);
  }


  HRESULT D3D9CommonBuffer::ValidateBufferProperties(const D3D9_BUFFER_DESC* pDesc) {
    if (unlikely(pDesc->Size == 0))
      return D3DERR_INVALIDCALL;

    // Neither vertex nor index buffers can be created in D3DPOOL_SCRATCH
    // or in D3DPOOL_MANAGED with D3DUSAGE_DYNAMIC.
    if (unlikely(pDesc->Pool == D3DPOOL_SCRATCH
             || (pDesc->Pool == D3DPOOL_MANAGED && (pDesc->Usage & D3DUSAGE_DYNAMIC))))
      return D3DERR_INVALIDCALL;

    // D3DUSAGE_AUTOGENMIPMAP, D3DUSAGE_DEPTHSTENCIL and D3DUSAGE_RENDERTARGET
    // are not permitted on index or vertex buffers.
    if (unlikely((pDesc->Usage & D3DUSAGE_AUTOGENMIPMAP)
              || (pDesc->Usage & D3DUSAGE_DEPTHSTENCIL)
              || (pDesc->Usage & D3DUSAGE_RENDERTARGET)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }


  void D3D9CommonBuffer::PreLoad() {
    if (IsPoolManaged(m_desc.Pool)) {
      auto lock = m_parent->LockDevice();

      if (NeedsUpload())
        m_parent->FlushBuffer(this);
    }
  }

  
  D3D9_COMMON_BUFFER_MAP_MODE D3D9CommonBuffer::DetermineMapMode(const D3D9Options* options) const {
    if (m_desc.Pool != D3DPOOL_DEFAULT)
      return D3D9_COMMON_BUFFER_MAP_MODE_BUFFER;

    // CSGO keeps vertex buffers locked across multiple frames and writes to it. It uses them for drawing without unlocking first.
    // Tests show that D3D9 DEFAULT + USAGE_DYNAMIC behaves like a directly mapped buffer even when unlocked.
    // DEFAULT + WRITEONLY does not behave like a directly mapped buffer EXCEPT if its locked at the moment.
    // That's annoying to implement so we just always directly map DEFAULT + WRITEONLY.
    if (!(m_desc.Usage & (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY)))
      return D3D9_COMMON_BUFFER_MAP_MODE_BUFFER;

    if (!options->allowDirectBufferMapping)
      return D3D9_COMMON_BUFFER_MAP_MODE_BUFFER;

    return D3D9_COMMON_BUFFER_MAP_MODE_DIRECT;
  }


  Rc<DxvkBuffer> D3D9CommonBuffer::CreateBuffer() const {
    DxvkBufferCreateInfo  info;
    info.size   = m_desc.Size;
    info.usage  = 0;
    info.stages = 0;
    info.access = 0;

    VkMemoryPropertyFlags memoryFlags = 0;

    if (m_desc.Type == D3DRTYPE_VERTEXBUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

      if (m_parent->SupportsSWVP()) {
        info.usage  |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.stages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        info.access |= VK_ACCESS_SHADER_WRITE_BIT;
      }
    }
    else if (m_desc.Type == D3DRTYPE_INDEXBUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }

    if (m_mapMode == D3D9_COMMON_BUFFER_MAP_MODE_DIRECT) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;

      memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

      if ((m_desc.Usage & (D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC)) == 0
        || DoPerDrawUpload()
        || m_parent->CanOnlySWVP()
        || m_parent->GetOptions()->cachedDynamicBuffers) {
        // Never use uncached memory on devices that support SWVP because we might end up reading from it.

        info.access |= VK_ACCESS_HOST_READ_BIT;
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      } else {
        memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      }
    }
    else {
      info.stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
      info.usage  |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.access |= VK_ACCESS_TRANSFER_WRITE_BIT;

      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    return m_parent->GetDXVKDevice()->createBuffer(info, memoryFlags);
  }


  Rc<DxvkBuffer> D3D9CommonBuffer::CreateStagingBuffer() const {
    DxvkBufferCreateInfo  info;
    info.size   = m_desc.Size;
    info.stages = VK_PIPELINE_STAGE_HOST_BIT
                | VK_PIPELINE_STAGE_TRANSFER_BIT;

    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    info.access = VK_ACCESS_HOST_WRITE_BIT
                | VK_ACCESS_TRANSFER_READ_BIT;

    if (!(m_desc.Usage & D3DUSAGE_WRITEONLY))
      info.access |= VK_ACCESS_HOST_READ_BIT;

    VkMemoryPropertyFlags memoryFlags = 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    return m_parent->GetDXVKDevice()->createBuffer(info, memoryFlags);
  }

}