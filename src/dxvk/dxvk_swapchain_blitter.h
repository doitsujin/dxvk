#pragma once

#include <functional>
#include <thread>
#include <unordered_map>

#include "./hud/dxvk_hud.h"

#include "../util/thread.h"

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_context.h"

namespace dxvk {

  /**
   * \brief Gamma control point
   */
  struct DxvkGammaCp {
    uint16_t r, g, b, a;
  };


  /**
   * \brief Swap chain blitter pipeline key
   *
   * Used to look up specific pipelines.
   */
  struct DxvkSwapchainPipelineKey {
    /// Input color space. If this does not match the output color
    /// space, the input will be converted to match the output.
    VkColorSpaceKHR srcSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    /// Source image sample count. Used to determine the shader to
    /// use, and passed to it via a spec constant.
    VkSampleCountFlagBits srcSamples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    /// Whether the source image uses an sRGB format. Relevant for
    /// automatic color space conversion.
    VkBool32 srcIsSrgb = VK_FALSE;
    /// Output color space.
    VkColorSpaceKHR dstSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    /// Output image format. Used as pipeline state, but also to
    /// determine the sRGB-ness of the format.
    VkFormat dstFormat = VK_FORMAT_UNDEFINED;
    /// Bit indicating whether the input and output dimensions match.
    VkBool32 needsBlit = VK_FALSE;
    /// Bit indicating whether a gamma curve is to be applied.
    VkBool32 needsGamma = VK_FALSE;
    /// Bit indicating whether the HUD needs to be composited
    VkBool32 compositeHud = VK_FALSE;
    /// Bit indicating whether the software cursor needs to be composited
    VkBool32 compositeCursor = VK_FALSE;

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(srcSpace));
      hash.add(uint32_t(srcSamples));
      hash.add(uint32_t(srcIsSrgb));
      hash.add(uint32_t(dstSpace));
      hash.add(uint32_t(dstFormat));
      hash.add(uint32_t(needsBlit));
      hash.add(uint32_t(needsGamma));
      hash.add(uint32_t(compositeHud));
      hash.add(uint32_t(compositeCursor));
      return hash;
    }

    bool eq(const DxvkSwapchainPipelineKey& other) const {
      return srcSpace == other.srcSpace
          && srcSamples == other.srcSamples
          && srcIsSrgb == other.srcIsSrgb
          && dstSpace == other.dstSpace
          && dstFormat == other.dstFormat
          && needsBlit == other.needsBlit
          && needsGamma == other.needsGamma
          && compositeHud == other.compositeHud
          && compositeCursor == other.compositeCursor;
    }
  };


  /**
   * \brief Swap chain cursor pipeline key
   */
  struct DxvkCursorPipelineKey {
    /// Output color space.
    VkColorSpaceKHR dstSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    /// Output image format. Used as pipeline state, but also to
    /// determine the sRGB-ness of the format.
    VkFormat dstFormat = VK_FORMAT_UNDEFINED;

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(dstSpace));
      hash.add(uint32_t(dstFormat));
      return hash;
    }

    bool eq(const DxvkCursorPipelineKey& other) const {
      return dstSpace == other.dstSpace
          && dstFormat == other.dstFormat;
    }
  };


  /**
   * \brief Swap chain blitter
   *
   * Provides common rendering code for blitting
   * rendered images to a swap chain image.
   */
  class DxvkSwapchainBlitter : public RcObject {
    
  public:

    DxvkSwapchainBlitter(
      const Rc<DxvkDevice>& device,
      const Rc<hud::Hud>&   hud);
    ~DxvkSwapchainBlitter();

    /**
     * \brief Begins recording presentation commands
     *
     * Sets up the swap chain image and all internal resources, and
     * blits the source image onto the swap chain appropriately.
     * The swap chain image will remain bound for rendering.
     * \param [in] ctx Context objects
     * \param [in] dstView Swap chain image view
     * \param [in] dstRect Destination rectangle
     * \param [in] srcView Image to present
     * \param [in] srcColorSpace Image color space
     * \param [in] srcRect Source rectangle to present
     */
    void present(
      const DxvkContextObjects& ctx,
      const Rc<DxvkImageView>&  dstView,
            VkRect2D            dstRect,
      const Rc<DxvkImageView>&  srcView,
            VkRect2D            srcRect);

    /**
     * \brief Sets gamma ramp
     *
     * If the number of control points is non-zero, this
     * will create a texture containing a gamma ramp that
     * will be used for presentation.
     * \param [in] cpCount Number of control points
     * \param [in] cpData Control point data
     */
    void setGammaRamp(
            uint32_t            cpCount,
      const DxvkGammaCp*        cpData);

    /**
     * \brief Sets software cursor texture
     *
     * The cursor image is assumed to be in sRGB color space.
     * \param [in] extent Texture size, in pixels
     * \param [in] format Texture format
     * \param [in] data Texture data. Assumed to be
     *    tightly packed according to the format.
     */
    void setCursorTexture(
            VkExtent2D          extent,
            VkFormat            format,
      const void*               data);

    /**
     * \brief Sets cursor position
     *
     * If the size does not match the texture size, the
     * cursor will be rendered with a linear filter.
     * \param [in] rect Cursor rectangle, in pixels
     */
    void setCursorPos(
            VkRect2D            rect);

  private:

    struct SpecConstants {
      VkSampleCountFlagBits sampleCount;
      VkBool32 gammaBound;
      VkColorSpaceKHR srcSpace;
      VkBool32 srcIsSrgb;
      VkColorSpaceKHR dstSpace;
      VkBool32 dstIsSrgb;
      VkBool32 compositeHud;
      VkBool32 compositeCursor;
    };

    struct PushConstants {
      VkOffset2D srcOffset;
      VkExtent2D srcExtent;
      VkOffset2D dstOffset;
      VkOffset2D cursorOffset;
      VkExtent2D cursorExtent;
    };

    struct CursorSpecConstants {
      VkColorSpaceKHR dstSpace;
      VkBool32 dstIsSrgb;
    };

    struct CursorPushConstants {
      VkExtent2D dstExtent;
      VkOffset2D cursorOffset;
      VkExtent2D cursorExtent;
    };

    struct ShaderModule {
      VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
      VkPipelineShaderStageCreateInfo stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    };

    Rc<DxvkDevice>      m_device;
    Rc<hud::Hud>        m_hud;

    ShaderModule        m_shaderVsBlit;
    ShaderModule        m_shaderFsCopy;
    ShaderModule        m_shaderFsBlit;
    ShaderModule        m_shaderFsMsResolve;
    ShaderModule        m_shaderFsMsBlit;

    ShaderModule        m_shaderVsCursor;
    ShaderModule        m_shaderFsCursor;

    dxvk::mutex         m_mutex;
    Rc<DxvkBuffer>      m_gammaBuffer;
    Rc<DxvkImage>       m_gammaImage;
    Rc<DxvkImageView>   m_gammaView;
    uint32_t            m_gammaCpCount = 0;

    Rc<DxvkBuffer>      m_cursorBuffer;
    Rc<DxvkImage>       m_cursorImage;
    Rc<DxvkImageView>   m_cursorView;
    VkRect2D            m_cursorRect = { };

    Rc<DxvkSampler>     m_samplerPresent;
    Rc<DxvkSampler>     m_samplerGamma;
    Rc<DxvkSampler>     m_samplerCursorLinear;
    Rc<DxvkSampler>     m_samplerCursorNearest;

    Rc<DxvkImage>       m_hudImage;
    Rc<DxvkImageView>   m_hudView;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_cursorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_cursorPipelineLayout = VK_NULL_HANDLE;

    std::unordered_map<DxvkSwapchainPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_pipelines;

    std::unordered_map<DxvkCursorPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_cursorPipelines;

    void performDraw(
      const DxvkContextObjects&         ctx,
      const Rc<DxvkImageView>&          dstView,
            VkRect2D                    dstRect,
      const Rc<DxvkImageView>&          srcView,
            VkRect2D                    srcRect,
            VkBool32                    composite);

    void renderHudImage(
      const DxvkContextObjects&         ctx,
            VkExtent3D                  extent);

    void createHudImage(
            VkExtent3D                  extent);

    void destroyHudImage();

    void renderCursor(
      const DxvkContextObjects&         ctx,
      const Rc<DxvkImageView>&          dstView);

    void uploadGammaImage(
      const DxvkContextObjects&         ctx);

    void uploadCursorImage(
      const DxvkContextObjects&         ctx);

    void uploadTexture(
      const DxvkContextObjects&         ctx,
      const Rc<DxvkImage>&              image,
      const Rc<DxvkBuffer>&             buffer);

    void createSampler();

    void createShaders();

    void createShaderModule(
            ShaderModule&               shader,
            VkShaderStageFlagBits       stage,
            size_t                      size,
      const uint32_t*                   code);

    VkDescriptorSetLayout createSetLayout();

    VkDescriptorSetLayout createCursorSetLayout();

    VkPipelineLayout createPipelineLayout();

    VkPipelineLayout createCursorPipelineLayout();

    VkPipeline createPipeline(
      const DxvkSwapchainPipelineKey&   key);

    VkPipeline getPipeline(
      const DxvkSwapchainPipelineKey&   key);

    VkPipeline createCursorPipeline(
      const DxvkCursorPipelineKey&      key);

    VkPipeline getCursorPipeline(
      const DxvkCursorPipelineKey&      key);

    static bool needsComposition(
      const Rc<DxvkImageView>&          dstView);

  };
  
}