#include "dxvk_meta_clear.h"
#include "dxvk_device.h"

#include <dxvk_clear_buffer_f.h>
#include <dxvk_clear_buffer_u.h>
#include <dxvk_clear_image1d_f.h>
#include <dxvk_clear_image1d_u.h>
#include <dxvk_clear_image1darr_f.h>
#include <dxvk_clear_image1darr_u.h>
#include <dxvk_clear_image2d_f.h>
#include <dxvk_clear_image2d_u.h>
#include <dxvk_clear_image2darr_f.h>
#include <dxvk_clear_image2darr_u.h>
#include <dxvk_clear_image3d_f.h>
#include <dxvk_clear_image3d_u.h>

namespace dxvk {
  
  DxvkMetaClearObjects::DxvkMetaClearObjects(DxvkDevice* device)
  : m_device(device) {
    // Create pipeline layouts using those descriptor set layouts
    m_clearBufPipeLayout = createPipelineLayout(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
    m_clearImgPipeLayout = createPipelineLayout(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    // Create the actual compute pipelines
    m_clearPipesF32.clearBuf = createPipeline(dxvk_clear_buffer_f, m_clearBufPipeLayout);
    m_clearPipesU32.clearBuf = createPipeline(dxvk_clear_buffer_u, m_clearBufPipeLayout);

    m_clearPipesF32.clearImg1D = createPipeline(dxvk_clear_image1d_f, m_clearImgPipeLayout);
    m_clearPipesU32.clearImg1D = createPipeline(dxvk_clear_image1d_u, m_clearImgPipeLayout);
    m_clearPipesF32.clearImg2D = createPipeline(dxvk_clear_image2d_f, m_clearImgPipeLayout);
    m_clearPipesU32.clearImg2D = createPipeline(dxvk_clear_image2d_u, m_clearImgPipeLayout);
    m_clearPipesF32.clearImg3D = createPipeline(dxvk_clear_image3d_f, m_clearImgPipeLayout);
    m_clearPipesU32.clearImg3D = createPipeline(dxvk_clear_image3d_u, m_clearImgPipeLayout);

    m_clearPipesF32.clearImg1DArray = createPipeline(dxvk_clear_image1darr_f, m_clearImgPipeLayout);
    m_clearPipesU32.clearImg1DArray = createPipeline(dxvk_clear_image1darr_u, m_clearImgPipeLayout);
    m_clearPipesF32.clearImg2DArray = createPipeline(dxvk_clear_image2darr_f, m_clearImgPipeLayout);
    m_clearPipesU32.clearImg2DArray = createPipeline(dxvk_clear_image2darr_u, m_clearImgPipeLayout);
  }
  
  
  DxvkMetaClearObjects::~DxvkMetaClearObjects() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearBuf, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearBuf, nullptr);
    
    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearImg1D, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearImg1D, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearImg2D, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearImg2D, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearImg3D, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearImg3D, nullptr);
    
    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearImg1DArray, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearImg1DArray, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesF32.clearImg2DArray, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_clearPipesU32.clearImg2DArray, nullptr);
  }
  
  
  DxvkMetaClearPipeline DxvkMetaClearObjects::getClearBufferPipeline(
          DxvkFormatFlags       formatFlags) const {
    DxvkMetaClearPipeline result = { };
    result.layout     = m_clearBufPipeLayout;
    result.pipeline   = m_clearPipesF32.clearBuf;
    
    if (formatFlags.any(DxvkFormatFlag::SampledUInt, DxvkFormatFlag::SampledSInt))
      result.pipeline = m_clearPipesU32.clearBuf;

    result.workgroupSize = VkExtent3D { 128, 1, 1 };
    return result;
  }
  
  
  DxvkMetaClearPipeline DxvkMetaClearObjects::getClearImagePipeline(
          VkImageViewType       viewType,
          DxvkFormatFlags       formatFlags) const {
    const auto& pipelines = formatFlags.any(DxvkFormatFlag::SampledUInt, DxvkFormatFlag::SampledSInt)
      ? m_clearPipesU32
      : m_clearPipesF32;

    auto pipeInfo = [&pipelines, viewType] () -> std::pair<VkPipeline, VkExtent3D> {
      switch (viewType) {
        case VK_IMAGE_VIEW_TYPE_1D:       return { pipelines.clearImg1D,      VkExtent3D { 64, 1, 1 } };
        case VK_IMAGE_VIEW_TYPE_2D:       return { pipelines.clearImg2D,      VkExtent3D {  8, 8, 1 } };
        case VK_IMAGE_VIEW_TYPE_3D:       return { pipelines.clearImg3D,      VkExtent3D {  4, 4, 4 } };
        case VK_IMAGE_VIEW_TYPE_1D_ARRAY: return { pipelines.clearImg1DArray, VkExtent3D { 64, 1, 1 } };
        case VK_IMAGE_VIEW_TYPE_2D_ARRAY: return { pipelines.clearImg2DArray, VkExtent3D {  8, 8, 1 } };
        default:                          return { VkPipeline(VK_NULL_HANDLE), VkExtent3D { 0, 0, 0, } };
      }
    }();

    DxvkMetaClearPipeline result = { };
    result.layout        = m_clearImgPipeLayout;
    result.pipeline      = pipeInfo.first;
    result.workgroupSize = pipeInfo.second;
    return result;
  }
  
  
  const DxvkPipelineLayout* DxvkMetaClearObjects::createPipelineLayout(
          VkDescriptorType        descriptorType) {
    DxvkDescriptorSetLayoutBinding bindInfo = { descriptorType, 1, VK_SHADER_STAGE_COMPUTE_BIT };

    return m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(DxvkMetaClearArgs), 1, &bindInfo);
  }


  VkPipeline DxvkMetaClearObjects::createPipeline(
          size_t                  size,
    const uint32_t*               code,
    const DxvkPipelineLayout*     layout) {
    util::DxvkBuiltInShaderStage stage = { };
    stage.code = code;
    stage.size = size;

    return m_device->createBuiltInComputePipeline(layout, stage);
  }

}
