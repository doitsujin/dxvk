#include "dxvk_device.h"
#include "dxvk_meta_copy.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_copy_buffer_image.h>
#include <dxvk_copy_color_1d.h>
#include <dxvk_copy_color_2d.h>
#include <dxvk_copy_color_ms.h>
#include <dxvk_copy_depth_1d.h>
#include <dxvk_copy_depth_2d.h>
#include <dxvk_copy_depth_ms.h>
#include <dxvk_copy_depth_stencil_1d.h>
#include <dxvk_copy_depth_stencil_2d.h>
#include <dxvk_copy_depth_stencil_ms.h>

namespace dxvk {

  DxvkMetaCopyRenderPass::DxvkMetaCopyRenderPass(
    const Rc<vk::DeviceFn>&   vkd,
    const Rc<DxvkImageView>&  dstImageView,
    const Rc<DxvkImageView>&  srcImageView,
    const Rc<DxvkImageView>&  srcStencilView,
          bool                discardDst)
  : m_vkd           (vkd),
    m_dstImageView  (dstImageView),
    m_srcImageView  (srcImageView),
    m_srcStencilView(srcStencilView),
    m_renderPass    (createRenderPass(discardDst)),
    m_framebuffer   (createFramebuffer()) {

  }
  

  DxvkMetaCopyRenderPass::~DxvkMetaCopyRenderPass() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_framebuffer, nullptr);
    m_vkd->vkDestroyRenderPass (m_vkd->device(), m_renderPass,  nullptr);
  }

  
  VkRenderPass DxvkMetaCopyRenderPass::createRenderPass(bool discard) const {
    auto aspect = m_dstImageView->info().aspect;

    VkPipelineStageFlags cpyStages = 0;
    VkAccessFlags        cpyAccess = 0;

    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = m_dstImageView->info().format;
    attachment.samples          = m_dstImageView->imageInfo().sampleCount;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout    = m_dstImageView->imageInfo().layout;
    attachment.finalLayout      = m_dstImageView->imageInfo().layout;

    if (discard) {
      attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    VkAttachmentReference attachmentRef;
    attachmentRef.attachment    = 0;
    attachmentRef.layout        = (aspect & VK_IMAGE_ASPECT_COLOR_BIT)
      ? m_dstImageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      : m_dstImageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    VkSubpassDescription subpass;
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = 0;
    subpass.pColorAttachments       = nullptr;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
      subpass.colorAttachmentCount  = 1;
      subpass.pColorAttachments     = &attachmentRef;

      cpyStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      cpyAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!discard)
        cpyAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
      subpass.pDepthStencilAttachment = &attachmentRef;

      cpyStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      cpyAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (!discard)
        cpyAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    // We have to be somewhat conservative here since we cannot assume
    // that the backend blocks stages that are only used for meta ops
    VkPipelineStageFlags extStages = m_dstImageView->imageInfo().stages | m_srcImageView->imageInfo().stages;
    VkAccessFlags        extAccess = m_dstImageView->imageInfo().access;

    std::array<VkSubpassDependency, 2> dependencies = {{
      { VK_SUBPASS_EXTERNAL, 0, extStages, cpyStages, 0,         cpyAccess, 0 },
      { 0, VK_SUBPASS_EXTERNAL, cpyStages, extStages, cpyAccess, extAccess, 0 },
    }};

    VkRenderPassCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = 1;
    info.pAttachments           = &attachment;
    info.subpassCount           = 1;
    info.pSubpasses             = &subpass;
    info.dependencyCount        = dependencies.size();
    info.pDependencies          = dependencies.data();

    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyRenderPass: Failed to create render pass");
    return result;
  }


  VkFramebuffer DxvkMetaCopyRenderPass::createFramebuffer() const {
    VkImageView             dstViewHandle   = m_dstImageView->handle();
    VkImageSubresourceRange dstSubresources = m_dstImageView->subresources();
    VkExtent3D              dstExtent       = m_dstImageView->mipLevelExtent(0);

    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = 1;
    fboInfo.pAttachments    = &dstViewHandle;
    fboInfo.width           = dstExtent.width;
    fboInfo.height          = dstExtent.height;
    fboInfo.layers          = dstSubresources.layerCount;
    
    VkFramebuffer result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyRenderPass: Failed to create target framebuffer");
    return result;
  }


  DxvkMetaCopyObjects::DxvkMetaCopyObjects(const DxvkDevice* device)
  : m_vkd         (device->vkd()),
    m_sampler     (createSampler()),
    m_color {
      createShaderModule(dxvk_copy_color_1d),
      createShaderModule(dxvk_copy_color_2d),
      createShaderModule(dxvk_copy_color_ms) },
    m_depth {
      createShaderModule(dxvk_copy_depth_1d),
      createShaderModule(dxvk_copy_depth_2d),
      createShaderModule(dxvk_copy_depth_ms) } {
    if (device->extensions().extShaderViewportIndexLayer) {
      m_shaderVert = createShaderModule(dxvk_fullscreen_layer_vert);
    } else {
      m_shaderVert = createShaderModule(dxvk_fullscreen_vert);
      m_shaderGeom = createShaderModule(dxvk_fullscreen_geom);
    }
    
    if (device->extensions().extShaderStencilExport) {
      m_depthStencil = {
        createShaderModule(dxvk_copy_depth_stencil_1d),
        createShaderModule(dxvk_copy_depth_stencil_2d),
        createShaderModule(dxvk_copy_depth_stencil_ms) };
    }
  }


  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() {
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_copyBufferImagePipeline.pipeHandle, nullptr);
    m_vkd->vkDestroyPipelineLayout(m_vkd->device(), m_copyBufferImagePipeline.pipeLayout, nullptr);
    m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), m_copyBufferImagePipeline.dsetLayout, nullptr);

    for (const auto& pair : m_pipelines) {
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.second.pipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pair.second.pipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout (m_vkd->device(), pair.second.dsetLayout, nullptr);
      m_vkd->vkDestroyRenderPass(m_vkd->device(), pair.second.renderPass, nullptr);
    }

    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.fragMs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.frag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.frag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depth.fragMs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depth.frag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depth.frag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.fragMs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.frag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.frag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderGeom,   nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderVert,   nullptr);
    
    m_vkd->vkDestroySampler(m_vkd->device(), m_sampler, nullptr);
  }


  VkFormat DxvkMetaCopyObjects::getCopyDestinationFormat(
          VkImageAspectFlags    dstAspect,
          VkImageAspectFlags    srcAspect,
          VkFormat              srcFormat) const {
    if (srcAspect == dstAspect)
      return srcFormat;
    
    if (dstAspect == VK_IMAGE_ASPECT_COLOR_BIT
     && srcAspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
      switch (srcFormat) {
        case VK_FORMAT_D16_UNORM:  return VK_FORMAT_R16_UNORM;
        case VK_FORMAT_D32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        default:                   return VK_FORMAT_UNDEFINED;
      }
    }

    if (dstAspect == VK_IMAGE_ASPECT_DEPTH_BIT
     && srcAspect == VK_IMAGE_ASPECT_COLOR_BIT) {
      switch (srcFormat) {
        case VK_FORMAT_R16_UNORM:  return VK_FORMAT_D16_UNORM;
        case VK_FORMAT_R32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        default:                   return VK_FORMAT_UNDEFINED;
      }
    }

    return VK_FORMAT_UNDEFINED;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getPipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat,
          VkSampleCountFlagBits dstSamples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaCopyPipelineKey key;
    key.viewType = viewType;
    key.format   = dstFormat;
    key.samples  = dstSamples;
    
    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyBufferImagePipeline() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_copyBufferImagePipeline.pipeHandle)
      m_copyBufferImagePipeline = createCopyBufferImagePipeline();

    return m_copyBufferImagePipeline;
  }
  
  
  VkSampler DxvkMetaCopyObjects::createSampler() const {
    VkSamplerCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.magFilter              = VK_FILTER_NEAREST;
    info.minFilter              = VK_FILTER_NEAREST;
    info.mipmapMode             = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.mipLodBias             = 0.0f;
    info.anisotropyEnable       = VK_FALSE;
    info.maxAnisotropy          = 1.0f;
    info.compareEnable          = VK_FALSE;
    info.compareOp              = VK_COMPARE_OP_ALWAYS;
    info.minLod                 = 0.0f;
    info.maxLod                 = 0.0f;
    info.borderColor            = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    
    VkSampler result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create sampler");
    return result;
  }

  
  VkShaderModule DxvkMetaCopyObjects::createShaderModule(
    const SpirvCodeBuffer&          code) const {
    VkShaderModuleCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.codeSize               = code.size();
    info.pCode                  = code.data();
    
    VkShaderModule result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create shader module");
    return result;
  }

  
  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyBufferImagePipeline() {
    DxvkMetaCopyPipeline pipeline;
    pipeline.renderPass = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    }};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo;
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.pNext = nullptr;
    setLayoutInfo.flags = 0;
    setLayoutInfo.bindingCount = bindings.size();
    setLayoutInfo.pBindings = bindings.data();

    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &setLayoutInfo, nullptr, &pipeline.dsetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DxvkCopyBufferImageArgs) };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = nullptr;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &pipeline.dsetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &pipelineLayoutInfo, nullptr, &pipeline.pipeLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");

    VkShaderModule shaderModule = createShaderModule(dxvk_copy_buffer_image);

    VkComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.layout = pipeline.pipeLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.pNext = nullptr;
    pipelineInfo.stage.flags = 0;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.stage.pSpecializationInfo = nullptr;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (m_vkd->vkCreateComputePipelines(m_vkd->device(), VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &pipeline.pipeHandle) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create compute pipeline");

    m_vkd->vkDestroyShaderModule(m_vkd->device(), shaderModule, nullptr);
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createPipeline(
    const DxvkMetaCopyPipelineKey&  key) {
    DxvkMetaCopyPipeline pipeline;
    pipeline.renderPass = this->createRenderPass(key);
    pipeline.dsetLayout = this->createDescriptorSetLayout(key);
    pipeline.pipeLayout = this->createPipelineLayout(pipeline.dsetLayout);
    pipeline.pipeHandle = this->createPipelineObject(key, pipeline.pipeLayout, pipeline.renderPass);
    return pipeline;
  }


  VkRenderPass DxvkMetaCopyObjects::createRenderPass(
    const DxvkMetaCopyPipelineKey&  key) const {
    auto aspect = imageFormatInfo(key.format)->aspectMask;

    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = key.format;
    attachment.samples          = key.samples;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout    = VK_IMAGE_LAYOUT_GENERAL;
    attachment.finalLayout      = VK_IMAGE_LAYOUT_GENERAL;
    
    VkAttachmentReference attachmentRef;
    attachmentRef.attachment    = 0;
    attachmentRef.layout        = (aspect & VK_IMAGE_ASPECT_COLOR_BIT)
      ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass;
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = 0;
    subpass.pColorAttachments       = nullptr;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
      subpass.colorAttachmentCount  = 1;
      subpass.pColorAttachments     = &attachmentRef;
    } else {
      subpass.pDepthStencilAttachment = &attachmentRef;
    }

    VkRenderPassCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = 1;
    info.pAttachments           = &attachment;
    info.subpassCount           = 1;
    info.pSubpasses             = &subpass;
    info.dependencyCount        = 0;
    info.pDependencies          = nullptr;

    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create render pass");
    return result;
  }

  
  VkDescriptorSetLayout DxvkMetaCopyObjects::createDescriptorSetLayout(
    const DxvkMetaCopyPipelineKey&  key) const {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;

    for (uint32_t i = 0; i < 2; i++) {
      bindings[i].binding             = i;
      bindings[i].descriptorType      = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[i].descriptorCount     = 1;
      bindings[i].stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings[i].pImmutableSamplers  = &m_sampler;
    }
    
    VkDescriptorSetLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.bindingCount           = 1;
    info.pBindings              = bindings.data();

    auto format = imageFormatInfo(key.format);

    if (format->aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      info.bindingCount = 2;
    
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");
    return result;
  }

  
  VkPipelineLayout DxvkMetaCopyObjects::createPipelineLayout(
          VkDescriptorSetLayout     descriptorSetLayout) const {
    VkPushConstantRange push;
    push.stageFlags             = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset                 = 0;
    push.size                   = sizeof(VkOffset2D);

    VkPipelineLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &push;
    
    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");
    return result;
  }
  

  VkPipeline DxvkMetaCopyObjects::createPipelineObject(
    const DxvkMetaCopyPipelineKey&  key,
          VkPipelineLayout          pipelineLayout,
          VkRenderPass              renderPass) {
    auto aspect = imageFormatInfo(key.format)->aspectMask;

    std::array<VkPipelineShaderStageCreateInfo, 3> stages;
    uint32_t stageCount = 0;
    
    VkPipelineShaderStageCreateInfo& vsStage = stages[stageCount++];
    vsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStage.pNext               = nullptr;
    vsStage.flags               = 0;
    vsStage.stage               = VK_SHADER_STAGE_VERTEX_BIT;
    vsStage.module              = m_shaderVert;
    vsStage.pName               = "main";
    vsStage.pSpecializationInfo = nullptr;
    
    if (m_shaderGeom) {
      VkPipelineShaderStageCreateInfo& gsStage = stages[stageCount++];
      gsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      gsStage.pNext               = nullptr;
      gsStage.flags               = 0;
      gsStage.stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
      gsStage.module              = m_shaderGeom;
      gsStage.pName               = "main";
      gsStage.pSpecializationInfo = nullptr;
    }
    
    VkPipelineShaderStageCreateInfo& psStage = stages[stageCount++];
    psStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    psStage.pNext               = nullptr;
    psStage.flags               = 0;
    psStage.stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    psStage.module              = VK_NULL_HANDLE;
    psStage.pName               = "main";
    psStage.pSpecializationInfo = nullptr;

    std::array<std::pair<const FragShaders*, VkImageAspectFlags>, 3> shaderSets = {{
      { &m_color,        VK_IMAGE_ASPECT_COLOR_BIT },
      { &m_depth,        VK_IMAGE_ASPECT_DEPTH_BIT },
      { &m_depthStencil, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT },
    }};

    const FragShaders* shaderSet = nullptr;
    
    for (const auto& pair : shaderSets) {
      if (pair.second == aspect)
        shaderSet = pair.first;
    }

    if (!shaderSet)
      throw DxvkError("DxvkMetaCopyObjects: Unsupported aspect mask");

    if (key.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
      psStage.module = shaderSet->frag1D;
    else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
      psStage.module = shaderSet->frag2D;
    else
      psStage.module = shaderSet->fragMs;
    
    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
    }};
    
    VkPipelineDynamicStateCreateInfo dynState;
    dynState.sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.pNext              = nullptr;
    dynState.flags              = 0;
    dynState.dynamicStateCount  = dynStates.size();
    dynState.pDynamicStates     = dynStates.data();
    
    VkPipelineVertexInputStateCreateInfo viState;
    viState.sType               = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viState.pNext               = nullptr;
    viState.flags               = 0;
    viState.vertexBindingDescriptionCount   = 0;
    viState.pVertexBindingDescriptions      = nullptr;
    viState.vertexAttributeDescriptionCount = 0;
    viState.pVertexAttributeDescriptions    = nullptr;
    
    VkPipelineInputAssemblyStateCreateInfo iaState;
    iaState.sType               = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaState.pNext               = nullptr;
    iaState.flags               = 0;
    iaState.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.primitiveRestartEnable = VK_FALSE;
    
    VkPipelineViewportStateCreateInfo vpState;
    vpState.sType               = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.pNext               = nullptr;
    vpState.flags               = 0;
    vpState.viewportCount       = 1;
    vpState.pViewports          = nullptr;
    vpState.scissorCount        = 1;
    vpState.pScissors           = nullptr;
    
    VkPipelineRasterizationStateCreateInfo rsState;
    rsState.sType               = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsState.pNext               = nullptr;
    rsState.flags               = 0;
    rsState.depthClampEnable    = VK_TRUE;
    rsState.rasterizerDiscardEnable = VK_FALSE;
    rsState.polygonMode         = VK_POLYGON_MODE_FILL;
    rsState.cullMode            = VK_CULL_MODE_NONE;
    rsState.frontFace           = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthBiasEnable     = VK_FALSE;
    rsState.depthBiasConstantFactor = 0.0f;
    rsState.depthBiasClamp          = 0.0f;
    rsState.depthBiasSlopeFactor    = 0.0f;
    rsState.lineWidth           = 1.0f;
    
    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState;
    msState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msState.pNext                 = nullptr;
    msState.flags                 = 0;
    msState.rasterizationSamples  = key.samples;
    msState.sampleShadingEnable   = key.samples != VK_SAMPLE_COUNT_1_BIT;
    msState.minSampleShading      = 1.0f;
    msState.pSampleMask           = &msMask;
    msState.alphaToCoverageEnable = VK_FALSE;
    msState.alphaToOneEnable      = VK_FALSE;
    
    VkPipelineColorBlendAttachmentState cbAttachment;
    cbAttachment.blendEnable         = VK_FALSE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask      =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbState;
    cbState.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.pNext               = nullptr;
    cbState.flags               = 0;
    cbState.logicOpEnable       = VK_FALSE;
    cbState.logicOp             = VK_LOGIC_OP_NO_OP;
    cbState.attachmentCount     = 1;
    cbState.pAttachments        = &cbAttachment;
    
    for (uint32_t i = 0; i < 4; i++)
      cbState.blendConstants[i] = 0.0f;
    
    VkStencilOpState stencilOp;
    stencilOp.failOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp       = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = 0xFFFFFFFF;
    stencilOp.writeMask         = 0xFFFFFFFF;
    stencilOp.reference         = 0;
    
    VkPipelineDepthStencilStateCreateInfo dsState;
    dsState.sType               = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsState.pNext               = nullptr;
    dsState.flags               = 0;
    dsState.depthTestEnable     = VK_TRUE;
    dsState.depthWriteEnable    = VK_TRUE;
    dsState.depthCompareOp      = VK_COMPARE_OP_ALWAYS;
    dsState.depthBoundsTestEnable = VK_FALSE;
    dsState.stencilTestEnable   = VK_TRUE;
    dsState.front               = stencilOp;
    dsState.back                = stencilOp;
    dsState.minDepthBounds      = 0.0f;
    dsState.maxDepthBounds      = 1.0f;
    
    VkGraphicsPipelineCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.stageCount             = stageCount;
    info.pStages                = stages.data();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pTessellationState     = nullptr;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = (aspect & VK_IMAGE_ASPECT_COLOR_BIT) ? &cbState : nullptr;
    info.pDepthStencilState     = (aspect & VK_IMAGE_ASPECT_COLOR_BIT) ? nullptr : &dsState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.renderPass             = renderPass;
    info.subpass                = 0;
    info.basePipelineHandle     = VK_NULL_HANDLE;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create graphics pipeline");
    return result;
  }
  
}