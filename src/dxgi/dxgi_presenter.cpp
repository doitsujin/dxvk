#include "dxgi_presenter.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  DxgiPresenter::DxgiPresenter(
    const Rc<DxvkDevice>&          device,
          HWND                     window)
  : m_device  (device),
    m_context (device->createContext()) {
    
    // Create Vulkan surface for the window
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(window, GWLP_HINSTANCE));
    
    m_surface = m_device->adapter()->createSurface(instance, window);
    
    // Reset options for the swap chain itself. We will
    // create a swap chain object before presentation.
    m_options.preferredSurfaceFormat = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    m_options.preferredPresentMode   = VK_PRESENT_MODE_FIFO_KHR;
    m_options.preferredBufferSize    = { 0u, 0u };
    
    // Sampler for presentation
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_NEAREST;
    samplerInfo.minFilter       = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 0.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor     = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.usePixelCoord   = VK_FALSE;
    m_samplerFitting = m_device->createSampler(samplerInfo);
    
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    m_samplerScaling = m_device->createSampler(samplerInfo);
    
    // Set up context state. The shader bindings and the
    // constant state objects will never be modified.
    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    m_context->setInputAssemblyState(iaState);
    
    m_context->setInputLayout(
      0, nullptr, 0, nullptr);
    
    DxvkRasterizerState rsState;
    rsState.enableDepthClamp   = VK_FALSE;
    rsState.enableDiscard      = VK_FALSE;
    rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthBiasEnable    = VK_FALSE;
    rsState.depthBiasConstant  = 0.0f;
    rsState.depthBiasClamp     = 0.0f;
    rsState.depthBiasSlope     = 0.0f;
    m_context->setRasterizerState(rsState);
    
    DxvkMultisampleState msState;
    msState.sampleMask            = 0xffffffff;
    msState.enableAlphaToCoverage = VK_FALSE;
    msState.enableAlphaToOne      = VK_FALSE;
    msState.enableSampleShading   = VK_FALSE;
    msState.minSampleShading      = 0.0f;
    m_context->setMultisampleState(msState);
    
    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask   = 0xFFFFFFFF;
    stencilOp.reference   = 0;
    
    DxvkDepthStencilState dsState;
    dsState.enableDepthTest   = VK_FALSE;
    dsState.enableDepthWrite  = VK_FALSE;
    dsState.enableDepthBounds = VK_FALSE;
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilOpFront    = stencilOp;
    dsState.stencilOpBack     = stencilOp;
    dsState.depthBoundsMin    = 0.0f;
    dsState.depthBoundsMax    = 1.0f;
    m_context->setDepthStencilState(dsState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_NO_OP;
    m_context->setLogicOpState(loState);
    
    m_blendMode.enableBlending  = VK_FALSE;
    m_blendMode.colorSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.colorDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.colorBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.alphaSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.alphaDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.alphaBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.writeMask       = VK_COLOR_COMPONENT_R_BIT
                                | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT
                                | VK_COLOR_COMPONENT_A_BIT;
    
    m_context->bindShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      this->createVertexShader());
    
    m_context->bindShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      this->createFragmentShader());
    
    m_hud = hud::Hud::createHud(m_device);
  }
  
  
  DxgiPresenter::~DxgiPresenter() {
    m_device->waitForIdle();
  }
  
  
  void DxgiPresenter::initBackBuffer(const Rc<DxvkImage>& image) {
    VkImageSubresourceRange sr;
    sr.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    sr.baseMipLevel   = 0;
    sr.levelCount     = image->info().mipLevels;
    sr.baseArrayLayer = 0;
    sr.layerCount     = image->info().numLayers;
    
    m_context->beginRecording(
      m_device->createCommandList());
    m_context->initImage(image, sr);
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }
  
  
  void DxgiPresenter::presentImage() {
    if (m_hud != nullptr) {
      m_hud->render({
        m_options.preferredBufferSize.width,
        m_options.preferredBufferSize.height,
      });
    }
    
    const bool fitSize =
        m_backBuffer->info().extent.width  == m_options.preferredBufferSize.width
     && m_backBuffer->info().extent.height == m_options.preferredBufferSize.height;
    
    m_context->beginRecording(
      m_device->createCommandList());
    
    VkImageSubresourceLayers resolveSubresources;
    resolveSubresources.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveSubresources.mipLevel        = 0;
    resolveSubresources.baseArrayLayer  = 0;
    resolveSubresources.layerCount      = 1;
    
    if (m_backBufferResolve != nullptr) {
      m_context->resolveImage(
        m_backBufferResolve, resolveSubresources,
        m_backBuffer,        resolveSubresources,
        VK_FORMAT_UNDEFINED);
    }
    
    const DxvkSwapSemaphores sem = m_swapchain->getSemaphorePair();
    
    auto framebuffer     = m_swapchain->getFramebuffer(sem.acquireSync);
    auto framebufferSize = framebuffer->size();
    
    m_context->bindFramebuffer(framebuffer);
    
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(framebufferSize.width);
    viewport.height   = static_cast<float>(framebufferSize.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor;
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = framebufferSize.width;
    scissor.extent.height = framebufferSize.height;
    
    m_context->setViewports(1, &viewport, &scissor);
    
    m_context->bindResourceSampler(BindingIds::Sampler,
      fitSize ? m_samplerFitting : m_samplerScaling);
    
    m_blendMode.enableBlending = VK_FALSE;
    m_context->setBlendMode(0, m_blendMode);
    
    m_context->bindResourceView(BindingIds::Texture, m_backBufferView, nullptr);
    m_context->draw(4, 1, 0, 0);
    
    if (m_hud != nullptr) {
      m_blendMode.enableBlending = VK_TRUE;
      m_context->setBlendMode(0, m_blendMode);
      
      m_context->bindResourceView(BindingIds::Texture, m_hud->texture(), nullptr);
      m_context->draw(4, 1, 0, 0);
    }
    
    m_device->submitCommandList(
      m_context->endRecording(),
      sem.acquireSync, sem.presentSync);
    
    m_swapchain->present(sem.presentSync);
  }
  
  
  void DxgiPresenter::updateBackBuffer(const Rc<DxvkImage>& image) {
    // Explicitly destroy the old stuff
    m_backBuffer        = image;
    m_backBufferResolve = nullptr;
    m_backBufferView    = nullptr;
    
    // If a multisampled back buffer was requested, we also need to
    // create a resolve image with otherwise identical properties.
    // Multisample images cannot be sampled from.
    if (image->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      DxvkImageCreateInfo resolveInfo;
      resolveInfo.type          = VK_IMAGE_TYPE_2D;
      resolveInfo.format        = image->info().format;
      resolveInfo.flags         = 0;
      resolveInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
      resolveInfo.extent        = image->info().extent;
      resolveInfo.numLayers     = 1;
      resolveInfo.mipLevels     = 1;
      resolveInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      resolveInfo.stages        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                | VK_PIPELINE_STAGE_TRANSFER_BIT;
      resolveInfo.access        = VK_ACCESS_SHADER_READ_BIT
                                | VK_ACCESS_TRANSFER_WRITE_BIT;
      resolveInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      resolveInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      m_backBufferResolve = m_device->createImage(
        resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    
    // Create an image view that allows the
    // image to be bound as a shader resource.
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = image->info().format;
    viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel   = 0;
    viewInfo.numLevels  = 1;
    viewInfo.minLayer   = 0;
    viewInfo.numLayers  = 1;
    
    m_backBufferView = m_device->createImageView(
      m_backBufferResolve != nullptr
        ? m_backBufferResolve
        : m_backBuffer,
      viewInfo);
    
    // TODO move this elsewhere
    this->initBackBuffer(m_backBuffer);
  }
  
  
  void DxgiPresenter::recreateSwapchain(const DxvkSwapchainProperties& options) {
    const bool doRecreate =
         options.preferredSurfaceFormat.format      != m_options.preferredSurfaceFormat.format
      || options.preferredSurfaceFormat.colorSpace  != m_options.preferredSurfaceFormat.colorSpace
      || options.preferredPresentMode               != m_options.preferredPresentMode
      || options.preferredBufferSize.width          != m_options.preferredBufferSize.width
      || options.preferredBufferSize.height         != m_options.preferredBufferSize.height;
    
    if (doRecreate) {
      Logger::info(str::format(
        "DxgiPresenter: Recreating swap chain: ",
        "\n  Format:       ", options.preferredSurfaceFormat.format,
        "\n  Present mode: ", options.preferredPresentMode,
        "\n  Buffer size:  ", options.preferredBufferSize.width, "x", options.preferredBufferSize.height));
      
      m_options = options;
      
      if (m_swapchain == nullptr) {
        m_swapchain = m_device->createSwapchain(
          m_surface, options);
      } else {
        m_swapchain->changeProperties(options);
      }
    }
  }
  
  
  VkSurfaceFormatKHR DxgiPresenter::pickSurfaceFormat(DXGI_FORMAT fmt) const {
    std::vector<VkSurfaceFormatKHR> formats;
    
    switch (fmt) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM: {
        formats.push_back({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
        formats.push_back({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
        formats.push_back({ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
        formats.push_back({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      default:
        Logger::warn(str::format("DxgiPresenter: Unknown format: ", fmt));
    }
    
    return m_surface->pickSurfaceFormat(
      formats.size(), formats.data());
  }
  
  
  VkPresentModeKHR DxgiPresenter::pickPresentMode(VkPresentModeKHR preferred) const {
    return m_surface->pickPresentMode(1, &preferred);
  }
  
  
  Rc<DxvkShader> DxgiPresenter::createVertexShader() {
    SpirvModule module;
    
    // Set up basic vertex shader capabilities
    module.enableCapability(spv::CapabilityShader);
    module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    
    // ID of the entry point (function)
    uint32_t entryPointId = module.allocateId();
    
    // Data type definitions
    uint32_t typeVoid       = module.defVoidType();
    uint32_t typeU32        = module.defIntType(32, 0);
    uint32_t typeF32        = module.defFloatType(32);
    uint32_t typeVec2       = module.defVectorType(typeF32, 2);
    uint32_t typeVec4       = module.defVectorType(typeF32, 4);
    uint32_t typeVec4Arr4   = module.defArrayType(typeVec4, module.constu32(4));
    uint32_t typeFn         = module.defFunctionType(typeVoid, 0, nullptr);
    
    // Pointer type definitions
    uint32_t ptrInputU32    = module.defPointerType(typeU32, spv::StorageClassInput);
    uint32_t ptrOutputVec2  = module.defPointerType(typeVec2, spv::StorageClassOutput);
    uint32_t ptrOutputVec4  = module.defPointerType(typeVec4, spv::StorageClassOutput);
    uint32_t ptrPrivateVec4 = module.defPointerType(typeVec4, spv::StorageClassPrivate);
    uint32_t ptrPrivateArr4 = module.defPointerType(typeVec4Arr4, spv::StorageClassPrivate);
    
    // Input variable: VertexIndex
    uint32_t inVertexId = module.newVar(
      ptrInputU32, spv::StorageClassInput);
    module.decorateBuiltIn(inVertexId, spv::BuiltInVertexIndex);
    
    // Output variable: Position
    uint32_t outPosition = module.newVar(
      ptrOutputVec4, spv::StorageClassOutput);
    module.decorateBuiltIn(outPosition, spv::BuiltInPosition);
    
    // Output variable: Texture coordinates
    uint32_t outTexCoord = module.newVar(
      ptrOutputVec2, spv::StorageClassOutput);
    module.decorateLocation(outTexCoord, 0);
    
    // Temporary variable: Vertex array
    uint32_t varVertexArray = module.newVar(
      ptrPrivateArr4, spv::StorageClassPrivate);
    
    // Scalar constants
    uint32_t constF32Zero   = module.constf32( 0.0f);
    uint32_t constF32Half   = module.constf32( 0.5f);
    uint32_t constF32Pos1   = module.constf32( 1.0f);
    uint32_t constF32Neg1   = module.constf32(-1.0f);
    
    // Vector constants
    uint32_t constVec2HalfIds[2] = { constF32Half, constF32Half };
    uint32_t constVec2Half  = module.constComposite(typeVec2, 2, constVec2HalfIds);
    
    // Construct vertex array
    uint32_t vertexData[16] = {
      constF32Neg1, constF32Neg1, constF32Zero, constF32Pos1,
      constF32Neg1, constF32Pos1, constF32Zero, constF32Pos1,
      constF32Pos1, constF32Neg1, constF32Zero, constF32Pos1,
      constF32Pos1, constF32Pos1, constF32Zero, constF32Pos1,
    };
    
    uint32_t vertexConstants[4] = {
      module.constComposite(typeVec4, 4, vertexData +  0),
      module.constComposite(typeVec4, 4, vertexData +  4),
      module.constComposite(typeVec4, 4, vertexData +  8),
      module.constComposite(typeVec4, 4, vertexData + 12),
    };
    
    uint32_t vertexArray = module.constComposite(
      typeVec4Arr4, 4, vertexConstants);
    
    
    // Function header
    module.functionBegin(typeVoid, entryPointId, typeFn, spv::FunctionControlMaskNone);
    module.opLabel(module.allocateId());
    module.opStore(varVertexArray, vertexArray);
    
    // Load position of the current vertex
    uint32_t tmpVertexId  = module.opLoad(typeU32, inVertexId);
    uint32_t tmpVertexPtr = module.opAccessChain(
      ptrPrivateVec4, varVertexArray, 1, &tmpVertexId);
    uint32_t tmpVertexPos = module.opLoad(typeVec4, tmpVertexPtr);
    module.opStore(outPosition, tmpVertexPos);
    
    // Compute texture coordinates
    uint32_t swizzleIndices[2] = { 0, 1 };
    uint32_t tmpTexCoord  = module.opVectorShuffle(typeVec2,
      tmpVertexPos, tmpVertexPos, 2, swizzleIndices);
    tmpTexCoord = module.opFMul(typeVec2, tmpTexCoord, constVec2Half);
    tmpTexCoord = module.opFAdd(typeVec2, tmpTexCoord, constVec2Half);
    module.opStore(outTexCoord, tmpTexCoord);
    
    module.opReturn();
    module.functionEnd();
    
    // Register function entry point
    std::array<uint32_t, 3> interfaces = {
      inVertexId, outPosition, outTexCoord,
    };
    
    module.addEntryPoint(entryPointId, spv::ExecutionModelVertex,
      "main", interfaces.size(), interfaces.data());
    
    // Create the actual shader module
    return m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0u, 1u },
      module.compile());
  }
  
  
  Rc<DxvkShader> DxgiPresenter::createFragmentShader() {
    SpirvModule module;
    
    module.enableCapability(spv::CapabilityShader);
    module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    
    uint32_t entryPointId = module.allocateId();
    
    // Data type definitions
    uint32_t typeVoid       = module.defVoidType();
    uint32_t typeF32        = module.defFloatType(32);
    uint32_t typeVec2       = module.defVectorType(typeF32, 2);
    uint32_t typeVec4       = module.defVectorType(typeF32, 4);
    uint32_t typeFn         = module.defFunctionType(typeVoid, 0, nullptr);
    uint32_t typeSampler    = module.defSamplerType();
    uint32_t typeTexture    = module.defImageType(
      typeF32, spv::Dim2D, 0, 0, 0, 1, spv::ImageFormatUnknown);
    uint32_t typeSampledTex = module.defSampledImageType(typeTexture);
    
    // Pointer type definitions
    uint32_t ptrInputVec2   = module.defPointerType(typeVec2, spv::StorageClassInput);
    uint32_t ptrOutputVec4  = module.defPointerType(typeVec4, spv::StorageClassOutput);
    uint32_t ptrSampler     = module.defPointerType(typeSampler, spv::StorageClassUniformConstant);
    uint32_t ptrTexture     = module.defPointerType(typeTexture, spv::StorageClassUniformConstant);
    
    // Sampler
    uint32_t rcSampler = module.newVar(ptrSampler, spv::StorageClassUniformConstant);
    module.decorateDescriptorSet(rcSampler, 0);
    module.decorateBinding(rcSampler, BindingIds::Sampler);
    
    // Texture
    uint32_t rcTexture = module.newVar(ptrTexture, spv::StorageClassUniformConstant);
    module.decorateDescriptorSet(rcTexture, 0);
    module.decorateBinding(rcTexture, BindingIds::Texture);
    
    // Input variable: Texture coordinates
    uint32_t inTexCoord = module.newVar(
      ptrInputVec2, spv::StorageClassInput);
    module.decorateLocation(inTexCoord, 0);
    
    // Output variable: Final color
    uint32_t outColor = module.newVar(
      ptrOutputVec4, spv::StorageClassOutput);
    module.decorateLocation(outColor, 0);
    
    // Function header
    module.functionBegin(typeVoid, entryPointId, typeFn, spv::FunctionControlMaskNone);
    module.opLabel(module.allocateId());
    
    // Load texture coordinates
    module.opStore(outColor,
      module.opImageSampleImplicitLod(
        typeVec4,
        module.opSampledImage(
          typeSampledTex,
          module.opLoad(typeTexture, rcTexture),
          module.opLoad(typeSampler, rcSampler)),
        module.opLoad(typeVec2, inTexCoord),
        SpirvImageOperands()));
    
    module.opReturn();
    module.functionEnd();
    
    // Register function entry point
    std::array<uint32_t, 2> interfaces = { inTexCoord, outColor };
    
    module.addEntryPoint(entryPointId, spv::ExecutionModelFragment,
      "main", interfaces.size(), interfaces.data());
    
    // Shader resource slots
    std::array<DxvkResourceSlot, 2> resourceSlots = {{
      { BindingIds::Sampler, VK_DESCRIPTOR_TYPE_SAMPLER,       VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { BindingIds::Texture, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_VIEW_TYPE_2D       },
    }};
    
    // Create the actual shader module
    return m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      resourceSlots.size(),
      resourceSlots.data(),
      { 1u, 1u },
      module.compile());
  }
  
}
