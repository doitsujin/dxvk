#pragma once

#include "dxvk_gpu_event.h"
#include "dxvk_gpu_query.h"
#include "dxvk_memory.h"
#include "dxvk_meta_blit.h"
#include "dxvk_meta_clear.h"
#include "dxvk_meta_copy.h"
#include "dxvk_meta_mipgen.h"
#include "dxvk_meta_pack.h"
#include "dxvk_meta_resolve.h"
#include "dxvk_pipemanager.h"
#include "dxvk_renderpass.h"
#include "dxvk_unbound.h"

#include "../util/util_lazy.h"

namespace dxvk {

  class DxvkObjects {

  public:

    DxvkObjects(DxvkDevice* device)
    : m_device          (device),
      m_memoryManager   (device),
      m_renderPassPool  (device),
      m_pipelineManager (device, &m_renderPassPool),
      m_eventPool       (device),
      m_queryPool       (device),
      m_dummyResources  (device) {

    }

    DxvkMemoryAllocator& memoryManager() {
      return m_memoryManager;
    }

    DxvkRenderPassPool& renderPassPool() {
      return m_renderPassPool;
    }

    DxvkPipelineManager& pipelineManager() {
      return m_pipelineManager;
    }

    DxvkGpuEventPool& eventPool() {
      return m_eventPool;
    }

    DxvkGpuQueryPool& queryPool() {
      return m_queryPool;
    }

    DxvkUnboundResources& dummyResources() {
      return m_dummyResources;
    }

    DxvkMetaBlitObjects& metaBlit() {
      return m_metaBlit.get(m_device);
    }

    DxvkMetaClearObjects& metaClear() {
      return m_metaClear.get(m_device);
    }

    DxvkMetaCopyObjects& metaCopy() {
      return m_metaCopy.get(m_device);
    }

    DxvkMetaResolveObjects& metaResolve() {
      return m_metaResolve.get(m_device);
    }
    
    DxvkMetaPackObjects& metaPack() {
      return m_metaPack.get(m_device);
    }

  private:

    DxvkDevice*                   m_device;

    DxvkMemoryAllocator           m_memoryManager;
    DxvkRenderPassPool            m_renderPassPool;
    DxvkPipelineManager           m_pipelineManager;

    DxvkGpuEventPool              m_eventPool;
    DxvkGpuQueryPool              m_queryPool;

    DxvkUnboundResources          m_dummyResources;

    Lazy<DxvkMetaBlitObjects>     m_metaBlit;
    Lazy<DxvkMetaClearObjects>    m_metaClear;
    Lazy<DxvkMetaCopyObjects>     m_metaCopy;
    Lazy<DxvkMetaResolveObjects>  m_metaResolve;
    Lazy<DxvkMetaPackObjects>     m_metaPack;

  };

}