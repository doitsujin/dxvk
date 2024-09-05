#pragma once

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_image.h"
#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  enum class DxvkPresentBlitFsType {
    Copy,
    Blit,
    Resolve
  };

  struct DxvkMetaPresentBlitPipelineKey {
    DxvkPresentBlitFsType fs;
    VkSampleCountFlagBits   srcSamples;
    VkSampleCountFlagBits   dstSamples;
    VkFormat                viewFormat;
    bool                    hasGammaView;

    bool eq(const DxvkMetaPresentBlitPipelineKey& other) const {
      return this->fs         == other.fs
        && this->srcSamples   == other.srcSamples
        && this->dstSamples   == other.dstSamples
        && this->viewFormat   == other.viewFormat
        && this->hasGammaView == other.hasGammaView;
    }

    size_t hash() const {
      DxvkHashState result;
      result.add(uint32_t(this->fs));
      result.add(uint32_t(this->srcSamples));
      result.add(uint32_t(this->dstSamples));
      result.add(uint32_t(this->viewFormat));
      result.add(uint32_t(this->hasGammaView));
      return result;
    }
  };


  struct DxvkMetaPresentBlitPipeline {
    VkDescriptorSetLayout dsetLayout;
    VkPipelineLayout pipeLayout;
    VkPipeline pipeHandle;
  };

  class DxvkDevice;

  class DxvkMetaPresentBlitObjects {

  public:

    DxvkMetaPresentBlitObjects(const DxvkDevice* device);
    ~DxvkMetaPresentBlitObjects();

    DxvkMetaPresentBlitPipeline getPipeline(
          DxvkPresentBlitFsType   fs,
          VkSampleCountFlagBits   srcSamples,
          VkSampleCountFlagBits   dstSamples,
          VkFormat                viewFormat,
          bool                    hasGammaView);

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    struct PresenterArgs {
      VkOffset2D srcOffset;
      union {
        VkExtent2D srcExtent;
        VkOffset2D dstOffset;
      };
    };

    VkSampler gammaSampler() const {
      return m_gammaSampler;
    }

    VkSampler srcSampler() const {
      return m_srcSampler;
    }

    Rc<DxvkImageView> createResolveImage(const Rc<DxvkDevice>& device, const DxvkImageCreateInfo& info);

  private:

    Rc<vk::DeviceFn>  m_vkd;

    VkSampler m_samplerCopy;
    VkSampler m_samplerBlit;

    VkShaderModule     m_vs;
    VkShaderModule     m_fsBlit;
    VkShaderModule     m_fsCopy;
    VkShaderModule     m_fsResolve;
    VkShaderModule     m_fsResolveAmd;

    VkSampler m_srcSampler;
    VkSampler m_gammaSampler;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkMetaPresentBlitPipelineKey,
      DxvkMetaPresentBlitPipeline,
      DxvkHash, DxvkEq> m_pipelines;

    void createShaders(const DxvkDevice* device);

    void createSamplers();

    VkPipeline createPipeline(
          DxvkPresentBlitFsType fs,
          VkSampleCountFlagBits   srcSamples,
          VkSampleCountFlagBits   dstSamples,
          VkFormat                viewFormat,
          bool                    hasGammaView,
          VkPipelineLayout        pipeLayout);

    DxvkMetaPresentBlitPipeline createPipeline(
      const DxvkMetaPresentBlitPipelineKey& key);

    VkDescriptorSetLayout createDescriptorSetLayout();

    VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

  };

}
