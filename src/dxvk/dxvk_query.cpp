#include "dxvk_query.h"

namespace dxvk {
    
  DxvkQuery::DxvkQuery(
    const Rc<vk::DeviceFn>& vkd,
          VkQueryType       type)
  : m_vkd(vkd), m_type(type) {
    VkQueryPoolCreateInfo info;
    info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.queryType  = type;
    info.queryCount = MaxNumQueryCountPerPool;
    info.pipelineStatistics
      = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
      | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
      | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
      | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
      | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
      | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
    
    if (m_vkd->vkCreateQueryPool(m_vkd->device(), &info, nullptr, &m_queryPool) != VK_SUCCESS)
      throw DxvkError("DXVK: Failed to create query pool");
  }
  
  
  DxvkQuery::~DxvkQuery() {
    m_vkd->vkDestroyQueryPool(
      m_vkd->device(), m_queryPool, nullptr);
  }
  
}