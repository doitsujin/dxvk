#include <array>

#include "d3d11_features.h"

namespace dxvk {

  D3D11DeviceFeatures::D3D11DeviceFeatures() {

  }


  D3D11DeviceFeatures::D3D11DeviceFeatures(
    const Rc<DxvkInstance>&     Instance,
    const Rc<DxvkAdapter>&      Adapter,
    const D3D11Options&         Options,
          D3D_FEATURE_LEVEL     FeatureLevel)
  : m_features    (Adapter->features()),
    m_properties  (Adapter->devicePropertiesExt()) {
    // Assume no TBDR. DXVK does not optimize for TBDR architectures
    // anyway, and D3D11 does not really provide meaningful support.
    m_architectureInfo.TileBasedDeferredRenderer          = FALSE;

    // D3D9 options. We unconditionally support all of these.
    m_d3d9Options.FullNonPow2TextureSupport               = TRUE;

    m_d3d9Options1.FullNonPow2TextureSupported            = TRUE;
    m_d3d9Options1.DepthAsTextureWithLessEqualComparisonFilterSupported = TRUE;
    m_d3d9Options1.SimpleInstancingSupported              = TRUE;
    m_d3d9Options1.TextureCubeFaceRenderTargetWithNonCubeDepthStencilSupported = TRUE;

    m_d3d9Shadow.SupportsDepthAsTextureWithLessEqualComparisonFilter = TRUE;

    m_d3d9SimpleInstancing.SimpleInstancingSupported      = TRUE;

    // D3D10 options. We unconditionally support compute shaders.
    m_d3d10Options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = TRUE;

    // D3D11.1 options. All of these are required for Feature Level 11_1.
    auto sharedResourceTier = DetermineSharedResourceTier(Adapter, FeatureLevel);

    bool hasDoublePrecisionSupport = m_features.core.features.shaderFloat64
                                  && m_features.core.features.shaderInt64;

    m_d3d11Options.DiscardAPIsSeenByDriver                = TRUE;
    m_d3d11Options.FlagsForUpdateAndCopySeenByDriver      = TRUE;
    m_d3d11Options.ClearView                              = TRUE;
    m_d3d11Options.CopyWithOverlap                        = TRUE;
    m_d3d11Options.ConstantBufferPartialUpdate            = TRUE;
    m_d3d11Options.ConstantBufferOffsetting               = TRUE;
    m_d3d11Options.MapNoOverwriteOnDynamicConstantBuffer  = TRUE;
    m_d3d11Options.MapNoOverwriteOnDynamicBufferSRV       = TRUE;
    m_d3d11Options.ExtendedResourceSharing                = sharedResourceTier > D3D11_SHARED_RESOURCE_TIER_0;

    if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0) {
      m_d3d11Options.OutputMergerLogicOp                  = m_features.core.features.logicOp;
      m_d3d11Options.MultisampleRTVWithForcedSampleCountOne = TRUE; // Not really
    }

    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0) {
      m_d3d11Options.UAVOnlyRenderingForcedSampleCount    = TRUE;
      m_d3d11Options.SAD4ShaderInstructions               = TRUE;
      m_d3d11Options.ExtendedDoublesShaderInstructions    = hasDoublePrecisionSupport;
    }

    // D3D11.2 options.
    auto tiledResourcesTier = DetermineTiledResourcesTier(FeatureLevel);
    m_d3d11Options1.TiledResourcesTier                    = tiledResourcesTier;
    m_d3d11Options1.MinMaxFiltering                       = tiledResourcesTier >= D3D11_TILED_RESOURCES_TIER_2;
    m_d3d11Options1.ClearViewAlsoSupportsDepthOnlyFormats = TRUE;

    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
      m_d3d11Options1.MapOnDefaultBuffers                 = TRUE;

    // D3D11.3 options
    m_d3d11Options2.TypedUAVLoadAdditionalFormats         = DetermineUavExtendedTypedLoadSupport(Adapter, FeatureLevel);
    m_d3d11Options2.ConservativeRasterizationTier         = DetermineConservativeRasterizationTier(FeatureLevel);
    m_d3d11Options2.TiledResourcesTier                    = tiledResourcesTier;
    m_d3d11Options2.StandardSwizzle                       = FALSE;
    m_d3d11Options2.UnifiedMemoryArchitecture             = FALSE;

    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
      m_d3d11Options2.MapOnDefaultTextures                = TRUE;

    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_1) {
      m_d3d11Options2.ROVsSupported                       = m_features.extFragmentShaderInterlock.fragmentShaderPixelInterlock;
      m_d3d11Options2.PSSpecifiedStencilRefSupported      = m_features.extShaderStencilExport;
    }

    // More D3D11.3 options
    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0) {
      m_d3d11Options3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer =
        m_features.vk12.shaderOutputViewportIndex &&
        m_features.vk12.shaderOutputLayer;
    }

    // D3D11.4 options
    m_d3d11Options4.ExtendedNV12SharedTextureSupported    = sharedResourceTier > D3D11_SHARED_RESOURCE_TIER_0;

    // More D3D11.4 options
    m_d3d11Options5.SharedResourceTier                    = sharedResourceTier;

    // Double-precision support
    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
      m_doubles.DoublePrecisionFloatShaderOps             = hasDoublePrecisionSupport;

    // These numbers are not accurate, but we have no real way to query these
    m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerResource = 32;
    m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerProcess = 40;

    // Marker support only depends on the debug utils extension
    m_marker.Profile = static_cast<bool>(Instance->extensions().extDebugUtils);

    // DXVK will keep all shaders in memory once created, and all Vulkan
    // drivers that we know of that can run DXVK have an on-disk cache.
    m_shaderCache.SupportFlags = D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_INPROC_CACHE
                               | D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE;

    // DXVK does not support min precision
    m_shaderMinPrecision.PixelShaderMinPrecision          = 0;
    m_shaderMinPrecision.AllOtherShaderStagesMinPrecision = 0;

    // Report native support for command lists by default. Deferred context
    // usage can be beneficial for us as ExecuteCommandList has low overhead,
    // and we avoid having to deal with known UpdateSubresource bugs this way.
    m_threading.DriverConcurrentCreates                   = TRUE;
    m_threading.DriverCommandLists                        = Options.exposeDriverCommandLists;
  }


  D3D11DeviceFeatures::~D3D11DeviceFeatures() {

  }


  HRESULT D3D11DeviceFeatures::GetFeatureData(
          D3D11_FEATURE         Feature,
          UINT                  FeatureDataSize,
          void*                 pFeatureData) const {
    switch (Feature) {
      case D3D11_FEATURE_ARCHITECTURE_INFO:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_architectureInfo);
      case D3D11_FEATURE_D3D9_OPTIONS:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Options);
      case D3D11_FEATURE_D3D9_OPTIONS1:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Options1);
      case D3D11_FEATURE_D3D9_SHADOW_SUPPORT:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Shadow);
      case D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9SimpleInstancing);
      case D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d10Options);
      case D3D11_FEATURE_D3D11_OPTIONS:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options);
      case D3D11_FEATURE_D3D11_OPTIONS1:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options1);
      case D3D11_FEATURE_D3D11_OPTIONS2:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options2);
      case D3D11_FEATURE_D3D11_OPTIONS3:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options3);
      case D3D11_FEATURE_D3D11_OPTIONS4:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options4);
      case D3D11_FEATURE_D3D11_OPTIONS5:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options5);
      case D3D11_FEATURE_DOUBLES:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_doubles);
      case D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_gpuVirtualAddress);
      case D3D11_FEATURE_MARKER_SUPPORT:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_marker);
      case D3D11_FEATURE_SHADER_CACHE:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_shaderCache);
      case D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_shaderMinPrecision);
      case D3D11_FEATURE_THREADING:
        return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_threading);
      default:
        Logger::err(str::format("D3D11: Unknown feature: ", Feature));
        return E_INVALIDARG;
    }
  }


  D3D_FEATURE_LEVEL D3D11DeviceFeatures::GetMaxFeatureLevel(
    const Rc<DxvkInstance>&     Instance,
    const Rc<DxvkAdapter>&      Adapter) {
    D3D11Options options(Instance->config());
    D3D11DeviceFeatures features(Instance, Adapter, options, D3D_FEATURE_LEVEL_12_1);
    return features.GetMaxFeatureLevel();
  }


  D3D11_CONSERVATIVE_RASTERIZATION_TIER D3D11DeviceFeatures::DetermineConservativeRasterizationTier(
          D3D_FEATURE_LEVEL     FeatureLevel) {
    if (FeatureLevel < D3D_FEATURE_LEVEL_11_1
     || !m_features.extConservativeRasterization)
      return D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED;

    // We don't really have a way to query uncertainty regions,
    // so just check degenerate triangle behaviour
    if (!m_properties.extConservativeRasterization.degenerateTrianglesRasterized)
      return D3D11_CONSERVATIVE_RASTERIZATION_TIER_1;

    // Inner coverage is required for Tier 3 support
    if (!m_properties.extConservativeRasterization.fullyCoveredFragmentShaderInputVariable)
      return D3D11_CONSERVATIVE_RASTERIZATION_TIER_2;

    return D3D11_CONSERVATIVE_RASTERIZATION_TIER_3;
  }


  D3D11_SHARED_RESOURCE_TIER D3D11DeviceFeatures::DetermineSharedResourceTier(
    const Rc<DxvkAdapter>&      Adapter,
          D3D_FEATURE_LEVEL     FeatureLevel) {
    static std::atomic<bool> s_errorShown = { false };

    // Lie about supporting Tier 1 since that's the
    // minimum required tier for Feature Level 11_1
    if (!Adapter->features().khrExternalMemoryWin32) {
      if (!s_errorShown.exchange(true))
        Logger::warn("D3D11DeviceFeatures: External memory features not supported");

      return D3D11_SHARED_RESOURCE_TIER_1;
    }

    // Check support for extended formats. Ignore multi-plane
    // formats here since driver support varies too much.
    std::array<VkFormat, 30> requiredFormats = {{
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_FORMAT_R32G32B32A32_UINT,
      VK_FORMAT_R32G32B32A32_SINT,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_UINT,
      VK_FORMAT_R16G16B16A16_SNORM,
      VK_FORMAT_R16G16B16A16_SINT,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_UINT_PACK32,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_R8G8B8A8_UINT,
      VK_FORMAT_R8G8B8A8_SNORM,
      VK_FORMAT_R8G8B8A8_SINT,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_R32_UINT,
      VK_FORMAT_R32_SINT,
      VK_FORMAT_R16_SFLOAT,
      VK_FORMAT_R16_UNORM,
      VK_FORMAT_R16_UINT,
      VK_FORMAT_R16_SNORM,
      VK_FORMAT_R16_SINT,
      VK_FORMAT_R8_UNORM,
      VK_FORMAT_R8_UINT,
      VK_FORMAT_R8_SNORM,
      VK_FORMAT_R8_SINT,
    }};

    bool allKmtHandlesSupported = true;
    bool allNtHandlesSupported = true;

    for (auto f : requiredFormats) {
      allKmtHandlesSupported &= CheckFormatSharingSupport(Adapter, f, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT);
      allNtHandlesSupported &= CheckFormatSharingSupport(Adapter, f, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
    }

    // Again, lie about at least tier 1 support
    if (!allKmtHandlesSupported) {
      if (!s_errorShown.exchange(true))
        Logger::warn("D3D11DeviceFeatures: Some formats not supported for resource sharing");
      return D3D11_SHARED_RESOURCE_TIER_1;
    }

    // Tier 2 requires all the above formats to be shareable
    // with NT handles in order to support D3D12 interop
    if (!allNtHandlesSupported)
      return D3D11_SHARED_RESOURCE_TIER_1;

    // Tier 3 additionally requires R11G11B10 to be
    // shareable with D3D12
    if (!CheckFormatSharingSupport(Adapter, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT))
      return D3D11_SHARED_RESOURCE_TIER_2;

    return D3D11_SHARED_RESOURCE_TIER_3;
  }


  D3D11_TILED_RESOURCES_TIER D3D11DeviceFeatures::DetermineTiledResourcesTier(
          D3D_FEATURE_LEVEL     FeatureLevel) {
    if (FeatureLevel < D3D_FEATURE_LEVEL_11_0
     || !m_features.core.features.sparseBinding
     || !m_features.core.features.sparseResidencyBuffer
     || !m_features.core.features.sparseResidencyImage2D
     || !m_features.core.features.sparseResidencyAliased
     || !m_properties.core.properties.sparseProperties.residencyStandard2DBlockShape)
      return D3D11_TILED_RESOURCES_NOT_SUPPORTED;

    if (FeatureLevel < D3D_FEATURE_LEVEL_11_1
     || !m_features.core.features.shaderResourceResidency
     || !m_features.core.features.shaderResourceMinLod
     || !m_features.vk12.samplerFilterMinmax
     || !m_properties.vk12.filterMinmaxSingleComponentFormats
     || !m_properties.core.properties.sparseProperties.residencyNonResidentStrict
     || m_properties.core.properties.sparseProperties.residencyAlignedMipSize)
      return D3D11_TILED_RESOURCES_TIER_1;

    if (!m_features.core.features.sparseResidencyImage3D
     || !m_properties.core.properties.sparseProperties.residencyStandard3DBlockShape)
      return D3D11_TILED_RESOURCES_TIER_2;

    return D3D11_TILED_RESOURCES_TIER_3;
  }


  BOOL D3D11DeviceFeatures::DetermineUavExtendedTypedLoadSupport(
    const Rc<DxvkAdapter>&      Adapter,
          D3D_FEATURE_LEVEL     FeatureLevel) {
    static const std::array<VkFormat, 18> s_formats = {{
      VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_R32_UINT,
      VK_FORMAT_R32_SINT,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_FORMAT_R32G32B32A32_UINT,
      VK_FORMAT_R32G32B32A32_SINT,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R16G16B16A16_UINT,
      VK_FORMAT_R16G16B16A16_SINT,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UINT,
      VK_FORMAT_R8G8B8A8_SINT,
      VK_FORMAT_R16_SFLOAT,
      VK_FORMAT_R16_UINT,
      VK_FORMAT_R16_SINT,
      VK_FORMAT_R8_UNORM,
      VK_FORMAT_R8_UINT,
      VK_FORMAT_R8_SINT,
    }};

    if (FeatureLevel < D3D_FEATURE_LEVEL_11_0)
      return FALSE;

    for (auto f : s_formats) {
      DxvkFormatFeatures features = Adapter->getFormatFeatures(f);
      VkFormatFeatureFlags2 imgFeatures = features.optimal | features.linear;

      if (!(imgFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT))
        return FALSE;
    }

    return TRUE;
  }


  BOOL D3D11DeviceFeatures::CheckFormatSharingSupport(
    const Rc<DxvkAdapter>&      Adapter,
          VkFormat              Format,
          VkExternalMemoryHandleTypeFlagBits HandleType) {
    DxvkFormatQuery query = { };
    query.format = Format;
    query.type = VK_IMAGE_TYPE_2D;
    query.tiling = VK_IMAGE_TILING_OPTIMAL;
    query.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    query.handleType = HandleType;

    constexpr VkExternalMemoryFeatureFlags featureMask
      = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT
      | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;

    auto limits = Adapter->getFormatLimits(query);
    return limits && (limits->externalFeatures & featureMask);
  }


  D3D_FEATURE_LEVEL D3D11DeviceFeatures::GetMaxFeatureLevel() const {
    // Check Feature Level 11_0 features
    if (!m_features.core.features.drawIndirectFirstInstance
     || !m_features.core.features.fragmentStoresAndAtomics
     || !m_features.core.features.multiDrawIndirect
     || !m_features.core.features.tessellationShader)
      return D3D_FEATURE_LEVEL_10_1;

    // Check Feature Level 11_1 features
    if (!m_d3d11Options.OutputMergerLogicOp
     || !m_features.core.features.vertexPipelineStoresAndAtomics)
      return D3D_FEATURE_LEVEL_11_0;

    // Check Feature Level 12_0 features
    if (m_d3d11Options2.TiledResourcesTier < D3D11_TILED_RESOURCES_TIER_2
     || !m_d3d11Options2.TypedUAVLoadAdditionalFormats)
      return D3D_FEATURE_LEVEL_11_1;

    // Check Feature Level 12_1 features
    if (!m_d3d11Options2.ConservativeRasterizationTier
     || !m_d3d11Options2.ROVsSupported)
      return D3D_FEATURE_LEVEL_12_0;

    return D3D_FEATURE_LEVEL_12_1;
  }

}
