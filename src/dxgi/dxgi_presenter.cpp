#include "dxgi_presenter.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  DxgiPresenter::DxgiPresenter(
    const Rc<DxvkDevice>& device,
          HWND            window,
          UINT            bufferWidth,
          UINT            bufferHeight)
  : m_device  (device),
    m_context (device->createContext()) {
    
    // Create Vulkan surface for the window
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(window, GWLP_HINSTANCE));
    
    m_surface = m_device->adapter()->createSurface(instance, window);
    
    // Create swap chain for the surface
    DxvkSwapchainProperties swapchainProperties;
    swapchainProperties.preferredSurfaceFormat.format     = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainProperties.preferredSurfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainProperties.preferredPresentMode              = VK_PRESENT_MODE_FIFO_KHR;
    swapchainProperties.preferredBufferSize.width         = bufferWidth;
    swapchainProperties.preferredBufferSize.height        = bufferHeight;
    
    m_swapchain = m_device->createSwapchain(
      m_surface, swapchainProperties);
    
    // Synchronization semaphores for swap chain operations
    m_acquireSync = m_device->createSemaphore();
    m_presentSync = m_device->createSemaphore();
    
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
    
    m_sampler = m_device->createSampler(samplerInfo);
    
    // Set up context state. The shader bindings and the
    // constant state objects will never be modified.
    m_context->bindGraphicsPipeline(createPipeline());
    
    m_context->setInputAssemblyState(
      new DxvkInputAssemblyState(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_FALSE));
    
    m_context->setInputLayout(
      new DxvkInputLayout(
        0, nullptr, 0, nullptr));
    
    m_context->setRasterizerState(
      new DxvkRasterizerState(
        VK_FALSE, VK_FALSE,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f));
    
    m_context->setMultisampleState(
      new DxvkMultisampleState(
        VK_SAMPLE_COUNT_1_BIT, 0xFFFFFFFF,
        VK_FALSE, VK_FALSE, VK_FALSE, 0.0f));
    
    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask   = 0xFFFFFFFF;
    stencilOp.reference   = 0;
    
    m_context->setDepthStencilState(
      new DxvkDepthStencilState(
        VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE,
        VK_COMPARE_OP_ALWAYS, stencilOp, stencilOp,
        0.0f, 1.0f));
    
    VkPipelineColorBlendAttachmentState blendAttachment;
    blendAttachment.blendEnable         = VK_FALSE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
                                        | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT
                                        | VK_COLOR_COMPONENT_A_BIT;
    
    m_context->setBlendState(
      new DxvkBlendState(
        VK_FALSE, VK_LOGIC_OP_NO_OP,
        1, &blendAttachment));
  }
  
  
  DxgiPresenter::~DxgiPresenter() {
    
  }
  
  
  void DxgiPresenter::initBackBuffer(const Rc<DxvkImage>& image) {
    m_context->beginRecording(
      m_device->createCommandList());
    m_context->initImage(image, nullptr);
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }
  
  
  void DxgiPresenter::presentImage(const Rc<DxvkImageView>& view) {
    m_context->beginRecording(
      m_device->createCommandList());
    
    auto framebuffer = m_swapchain->getFramebuffer(m_acquireSync);
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
    
    m_context->bindResourceSampler(
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      BindingIds::Sampler, m_sampler);
    m_context->bindResourceImage(
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      BindingIds::Texture, view);
    m_context->draw(4, 1, 0, 0);
    
    m_device->submitCommandList(
      m_context->endRecording(),
      m_acquireSync, m_presentSync);
    
    m_swapchain->present(m_presentSync);
    
    // FIXME Make sure that the semaphores and the command
    // list can be safely used without stalling the device.
    m_device->waitForIdle();
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
      VK_SHADER_STAGE_VERTEX_BIT, module.compile());
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
        module.opLoad(typeVec2, inTexCoord)));
    
    module.opReturn();
    module.functionEnd();
    
    
    // Register function entry point
    std::array<uint32_t, 2> interfaces = { inTexCoord, outColor };
    
    module.addEntryPoint(entryPointId, spv::ExecutionModelFragment,
      "main", interfaces.size(), interfaces.data());
    
    
    // Create the actual shader module
    return m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT, module.compile());
  }
  
  
  Rc<DxvkBindingLayout> DxgiPresenter::createBindingLayout() {
    std::array<DxvkDescriptorSlot, 2> bindings;
    bindings.at(BindingIds::Sampler).slot   = BindingIds::Sampler;
    bindings.at(BindingIds::Sampler).type   = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings.at(BindingIds::Sampler).stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    bindings.at(BindingIds::Texture).slot   = BindingIds::Texture;
    bindings.at(BindingIds::Texture).type   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings.at(BindingIds::Texture).stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    return m_device->createBindingLayout(
      bindings.size(), bindings.data());
  }
  
  
  Rc<DxvkGraphicsPipeline> DxgiPresenter::createPipeline() {
    const Rc<DxvkShader> vs = this->createVertexShader();
    const Rc<DxvkShader> fs = this->createFragmentShader();
    
    return m_device->createGraphicsPipeline(
      this->createBindingLayout(),
      vs, nullptr, nullptr, nullptr, fs);
  }
  
}
