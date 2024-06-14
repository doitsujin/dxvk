#pragma once

#include "d3d11_include.h"
#include "d3d11_options.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_instance.h"

namespace dxvk {

  /**
   * \brief Device features
   *
   * Stores D3D device feature structs.
   */
  class D3D11DeviceFeatures {

  public:

    D3D11DeviceFeatures();

    D3D11DeviceFeatures(
      const Rc<DxvkInstance>&     Instance,
      const Rc<DxvkAdapter>&      Adapter,
      const D3D11Options&         Options,
            D3D_FEATURE_LEVEL     FeatureLevel);

    ~D3D11DeviceFeatures();

    /**
     * \brief Retrieves feature support data
     *
     * \param [in] Feature D3D feature to query
     * \param [in] FeatureDataSize Data size, in bytes
     * \param [out] pFeatureData Data
     * \returns Status of the operation
     */
    HRESULT GetFeatureData(
            D3D11_FEATURE         Feature,
            UINT                  FeatureDataSize,
            void*                 pFeatureData) const;

    /**
     * \brief Queries tiled resources tier
     * \returns Tiled resources tier
     */
    D3D11_TILED_RESOURCES_TIER GetTiledResourcesTier() const {
      return m_d3d11Options2.TiledResourcesTier;
    }

    /**
     * \brief Queries conservative rasterization tier
     * \returns Conservative rasterization tier
     */
    D3D11_CONSERVATIVE_RASTERIZATION_TIER GetConservativeRasterizationTier() const {
      return m_d3d11Options2.ConservativeRasterizationTier;
    }

    /**
     * \brief Tests maximum supported feature level
     *
     * \param [in] Instance DXVK instance
     * \param [in] Adapter DXVK adapter
     * \returns Highest supported feature level
     */
    static D3D_FEATURE_LEVEL GetMaxFeatureLevel(
      const Rc<DxvkInstance>&     Instance,
      const Rc<DxvkAdapter>&      Adapter);

  private:

    DxvkDeviceFeatures  m_features;
    DxvkDeviceInfo      m_properties;

    D3D11_FEATURE_DATA_ARCHITECTURE_INFO              m_architectureInfo            = { };
    D3D11_FEATURE_DATA_D3D9_OPTIONS                   m_d3d9Options                 = { };
    D3D11_FEATURE_DATA_D3D9_OPTIONS1                  m_d3d9Options1                = { };
    D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT            m_d3d9Shadow                  = { };
    D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT m_d3d9SimpleInstancing        = { };
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS       m_d3d10Options                = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS                  m_d3d11Options                = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS1                 m_d3d11Options1               = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS2                 m_d3d11Options2               = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS3                 m_d3d11Options3               = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS4                 m_d3d11Options4               = { };
    D3D11_FEATURE_DATA_D3D11_OPTIONS5                 m_d3d11Options5               = { };
    D3D11_FEATURE_DATA_DOUBLES                        m_doubles                     = { };
    D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT    m_gpuVirtualAddress           = { };
    D3D11_FEATURE_DATA_MARKER_SUPPORT                 m_marker                      = { };
    D3D11_FEATURE_DATA_SHADER_CACHE                   m_shaderCache                 = { };
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT   m_shaderMinPrecision          = { };
    D3D11_FEATURE_DATA_THREADING                      m_threading                   = { };

    template<typename T>
    static HRESULT GetTypedFeatureData(UINT Size, void* pDstData, const T* pSrcData) {
      if (Size != sizeof(T))
        return E_INVALIDARG;

      *(reinterpret_cast<T*>(pDstData)) = *pSrcData;
      return S_OK;
    }

    D3D11_CONSERVATIVE_RASTERIZATION_TIER DetermineConservativeRasterizationTier(
            D3D_FEATURE_LEVEL     FeatureLevel);

    D3D11_SHARED_RESOURCE_TIER DetermineSharedResourceTier(
      const Rc<DxvkAdapter>&      Adapter,
            D3D_FEATURE_LEVEL     FeatureLevel);

    D3D11_TILED_RESOURCES_TIER DetermineTiledResourcesTier(
            D3D_FEATURE_LEVEL     FeatureLevel);

    BOOL DetermineUavExtendedTypedLoadSupport(
      const Rc<DxvkAdapter>&      Adapter,
            D3D_FEATURE_LEVEL     FeatureLevel);

    BOOL CheckFormatSharingSupport(
      const Rc<DxvkAdapter>&      Adapter,
            VkFormat              Format,
            VkExternalMemoryHandleTypeFlagBits HandleType);

    D3D_FEATURE_LEVEL GetMaxFeatureLevel() const;

  };

}