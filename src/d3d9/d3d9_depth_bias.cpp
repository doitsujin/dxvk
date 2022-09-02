#include "d3d9_depth_bias.h"
#include "d3d9_find_depth_bias_factor.h"
#include "../dxvk/dxvk_format.h"

namespace dxvk {

  D3D9DepthBias::D3D9DepthBias(const Rc<DxvkDevice>& Device)
    : m_device(Device), m_context(Device->createContext(DxvkContextType::Supplementary)) {
    DxvkBufferCreateInfo readbackBufferInfo;
    readbackBufferInfo.size   = 4;
    readbackBufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    readbackBufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT;
    readbackBufferInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    m_readbackBuffer = m_device->createBuffer(readbackBufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    uint32_t* ptr = reinterpret_cast<uint32_t*>(m_readbackBuffer->mapPtr(0));
    *ptr = 0;

    DxvkShaderCreateInfo shaderInfo;
    shaderInfo.stage           = VK_SHADER_STAGE_VERTEX_BIT;
    shaderInfo.bindingCount    = 0;
    shaderInfo.bindings        = nullptr;
    shaderInfo.pushConstOffset = 0;
    shaderInfo.pushConstSize   = 0;
    shaderInfo.inputMask       = 0;
    m_vertexShader = new DxvkShader(shaderInfo, d3d9_find_depth_bias_factor);

    DetermineFactors();

    m_readbackBuffer = nullptr;
    m_vertexShader   = nullptr;
    m_context        = nullptr;
    m_device         = nullptr;
  }

  void D3D9DepthBias::DetermineFactors() {
    std::array<VkFormat, 5> depthFormats = {
      VK_FORMAT_D16_UNORM,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    Rc<DxvkAdapter> adapter = m_device->adapter();
    for (VkFormat format : depthFormats) {
      DxvkFormatFeatures supported = adapter->getFormatFeatures(format);
      if (!(supported.optimal & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT))
        continue;

      m_depthBiasFactors[GetFormatIndex(format)] = DetermineFixedFactor(format);
    }
  }

  uint32_t D3D9DepthBias::GetFormatIndex(VkFormat Format) {
    switch (Format) {
      case VK_FORMAT_D16_UNORM:
        return 0;
      case VK_FORMAT_D16_UNORM_S8_UINT:
        return 1;
      case VK_FORMAT_D24_UNORM_S8_UINT:
        return 2;
      case VK_FORMAT_D32_SFLOAT:
        return 3;
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return 4;
      default:
        return std::numeric_limits<uint32_t>::max();
    }
  }

  uint32_t D3D9DepthBias::DetermineFixedFactor(VkFormat Format) {
    /*
    * Depth bias in Vulkan is defined as:
    * o = dbclamp(m * depthBiasSlopeFactor + r * depthBiasConstantFactor)
    *
    * Depth bias in D3D9 is defined as:
    * o = dbclamp(m * depthBiasSlopeFactor + depthBiasConstantFactor)
    *
    * By rendering a pixel at depth 0 with a depthBiasConstantFactor of 1,
    * we can read back the value for r.
    * This is trivial for fixed point formats where r is a constant factor that is at most:
    * r = 2 * 2^(-n)
    * Floating point formats are more problematic where r depends on the maximum exponent e of a given primitive.
    * If n is the number of bits in the mantissa, r is defined as:
    * r = 2^(e-n)
    * As a best effort we calculate r for the exponent 0.5. To do that we use a triangle
    * that spans from z=0 at pixel 0, 0 to z = 1 at pixel 5,0.
    * By shifting the viewport by half a pixel, we make it sample at the left corner of the pixel
    * to ensure we get a clean z = 0.
    */

    DxvkImageCreateInfo dsCreateInfo;
    dsCreateInfo.type        = VK_IMAGE_TYPE_2D;
    dsCreateInfo.format      = Format;
    dsCreateInfo.flags       = 0;
    dsCreateInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    dsCreateInfo.extent      = { 5, 1, 1};
    dsCreateInfo.numLayers   = 1;
    dsCreateInfo.mipLevels   = 1;
    dsCreateInfo.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    dsCreateInfo.stages      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dsCreateInfo.access      = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    dsCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    dsCreateInfo.layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    Rc<DxvkImage> ds = m_device->createImage(dsCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewCreateInfo dsvCreateInfo;
    dsvCreateInfo.usage     = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    dsvCreateInfo.numLevels = 1;
    dsvCreateInfo.numLayers = 1;
    dsvCreateInfo.aspect    = VK_IMAGE_ASPECT_DEPTH_BIT;
    dsvCreateInfo.format    = Format;
    Rc<DxvkImageView> dsv = m_device->createImageView(ds, dsvCreateInfo);

    m_context->beginRecording(m_device->createCommandList());

    VkViewport viewport;
    viewport.x        = 0.5f; // Shift viewport so 0,0 is sampled at the top left of the pixel
    viewport.y        = 0.0f;
    viewport.width    = 5.0f;
    viewport.height   = 1.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor;
    scissor.offset    = { 0, 0   };
    scissor.extent    = { 5, 1 };
    m_context->setViewports(1, &viewport, &scissor);

    DxvkDepthBias biases;
    biases.depthBiasConstant = 1.0f;
    biases.depthBiasSlope    = 0.0f;
    biases.depthBiasClamp    = 0.0f;
    m_context->setDepthBias(biases);

    DxvkDepthBounds bounds;
    bounds.enableDepthBounds = VK_TRUE;
    bounds.minDepthBounds    = 0.0f;
    bounds.maxDepthBounds    = 1.0f;
    m_context->setDepthBounds(bounds);

    VkClearValue clearValue;
    clearValue.depthStencil.depth   = 0;
    clearValue.depthStencil.stencil = 0;
    m_context->clearRenderTarget(dsv, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, clearValue);

    DxvkRenderTargets rts = { };
    rts.depth.view   = dsv;
    rts.depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    m_context->bindRenderTargets(std::move(rts), 0);

    DxvkVertexAttribute attribute = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
    DxvkVertexBinding   binding   = { 0, 0, VK_VERTEX_INPUT_RATE_VERTEX, 16 };
    m_context->setInputLayout(1, &attribute, 1, &binding);

    DxvkRasterizerState rsState;
    rsState.polygonMode      = VK_POLYGON_MODE_FILL;
    rsState.cullMode         = VK_CULL_MODE_NONE;
    rsState.frontFace        = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthClipEnable  = VK_FALSE;
    rsState.depthBiasEnable  = VK_TRUE;
    rsState.conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    rsState.sampleCount      = VK_SAMPLE_COUNT_1_BIT;
    rsState.flatShading      = VK_FALSE;
    m_context->setRasterizerState(rsState);

    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    m_context->setInputAssemblyState(iaState);

    DxvkDepthStencilState dsState;
    dsState.enableDepthTest   = VK_TRUE;
    dsState.enableDepthWrite  = VK_TRUE;
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilOpFront    = {};
    dsState.stencilOpBack     = {};
    m_context->setDepthStencilState(dsState);

    DxvkMultisampleState msState;
    msState.sampleMask            = 0xFFFFFFFF;
    msState.enableAlphaToCoverage = false;
    m_context->setMultisampleState(msState);

    Rc<DxvkShader> shader = m_vertexShader;
    m_context->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(std::move(shader));
    m_context->draw(3, 1, 0, 0);

    m_context->copyImageToBuffer(
      m_readbackBuffer, 0, 0, 0, ds,
      { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, { 0, 0, 0 }, { 1, 1, 1}
    );
    m_context->flushCommandList();
    m_device->waitForResource(m_readbackBuffer, DxvkAccess::Read);


    float depthValue;

    switch (Format) {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_D16_UNORM_S8_UINT: {
        uint32_t maxValue = 1 << 16;
        const uint16_t* ptr = reinterpret_cast<uint16_t*>(m_readbackBuffer->mapPtr(0));
        depthValue = float(*ptr) / float(maxValue);
      } break;

      case VK_FORMAT_D24_UNORM_S8_UINT: {
        uint32_t maxValue = 1 << 24;
        const uint32_t* ptr = reinterpret_cast<uint32_t*>(m_readbackBuffer->mapPtr(0));
        depthValue = float(*ptr & 0xFFFFFF) / float(maxValue);
      } break;

      case VK_FORMAT_D32_SFLOAT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT: {
        const float* ptr = reinterpret_cast<float*>(m_readbackBuffer->mapPtr(0));
        depthValue = *ptr;
      } break;

      default: {
        depthValue = 1.0f / float(1 << 23);
      } break;
    }

    // Find the closest power of two to make up for any inaccuracies
    uint32_t low  = 0;
    uint32_t high = 32;
    uint32_t best = 0;
    while (high - low > 1) {
      uint32_t current = (low + high) / 2;
      float val = 1.0f / float(1 << current);
      if (abs(val - depthValue) < abs(1.0f / float(1 << best) - depthValue)) {
        best = current;
      }
      if (val > depthValue) {
        low = current;
      } else {
        high = current;
      }
    }

    Logger::info(str::format("Using depth bias r-factor: 1<<", best, " for format: ", Format));
    return float(1 << best);
  }

  float D3D9DepthBias::GetFactor(VkFormat Format) {
    uint32_t index = GetFormatIndex(Format);
    if (unlikely(index == std::numeric_limits<uint32_t>::max())) {
      return float(1 << 23);
    }
    return m_depthBiasFactors[index];
  }

}
