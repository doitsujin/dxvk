#pragma once

#include "../dxvk_device.h"

#include "dxvk_hud_font.h"

namespace dxvk::hud {
  
  /**
   * \brief HUD coordinates
   * 
   * Coordinates relative to the top-left
   * corner of the swap image, in pixels.
   */
  struct HudPos {
    float x;
    float y;
  };
  
  /**
   * \brief Color
   * 
   * SRGB color with alpha channel. The text
   * will use this color for the most part.
   */
  struct HudColor {
    float r;
    float g;
    float b;
    float a;
  };

  /**
   * \brief Normalized color
   *
   * SRGB color with alpha channel.
   */
  struct HudNormColor {
    uint8_t a;
    uint8_t b;
    uint8_t g;
    uint8_t r;
  };
  
  /**
   * \brief Graph point with color
   */
  struct HudGraphPoint {
    float         value;
    HudNormColor  color;
  };

  /**
   * \brief HUD push constant data
   */
  struct HudTextPushConstants {
    HudColor color;
    HudPos pos;
    uint32_t offset;
    float size;
    HudPos scale;
  };

  struct HudGraphPushConstants {
    uint32_t offset;
    uint32_t count;
    HudPos pos;
    HudPos size;
    HudPos scale;
    float  opacity;
  };

  /**
   * \brief Glyph data
   */
  struct HudGlyphGpuData {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t originX;
    int16_t originY;
  };

  struct HudFontGpuData {
    float size;
    float advance;
    uint32_t padding[2];
    HudGlyphGpuData glyphs[256];
  };

  /**
   * \brief Text renderer for the HUD
   * 
   * Can be used by the presentation backend to
   * display performance and driver information.
   */
  class HudRenderer {
    constexpr static VkDeviceSize DataBufferSize = 16384;
  public:
    
    HudRenderer(
      const Rc<DxvkDevice>&   device);
    
    ~HudRenderer();
    
    void beginFrame(
      const Rc<DxvkContext>&  context,
            VkExtent2D        surfaceSize,
            float             scale,
            float             opacity);
    
    void drawText(
            float             size,
            HudPos            pos,
            HudColor          color,
      const std::string&      text);
    
    void drawGraph(
            HudPos            pos,
            HudPos            size,
            size_t            pointCount,
      const HudGraphPoint*    pointData);
    
    VkExtent2D surfaceSize() const {
      return m_surfaceSize;
    }

    float scale() const {
      return m_scale;
    }
    
  private:
    
    enum class Mode {
      RenderNone,
      RenderText,
      RenderGraph,
    };

    struct ShaderPair {
      Rc<DxvkShader> vert;
      Rc<DxvkShader> frag;
    };
    
    Mode                m_mode;
    float               m_scale;
    float               m_opacity;
    VkExtent2D          m_surfaceSize;

    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    
    ShaderPair          m_textShaders;
    ShaderPair          m_graphShaders;
    
    Rc<DxvkBuffer>      m_dataBuffer;
    Rc<DxvkBufferView>  m_dataView;
    VkDeviceSize        m_dataOffset;

    Rc<DxvkBuffer>      m_fontBuffer;
    Rc<DxvkBufferView>  m_fontBufferView;
    Rc<DxvkImage>       m_fontImage;
    Rc<DxvkImageView>   m_fontView;
    Rc<DxvkSampler>     m_fontSampler;

    bool                m_initialized = false;

    void beginTextRendering();
    
    void beginGraphRendering();

    VkDeviceSize allocDataBuffer(VkDeviceSize size);

    ShaderPair createTextShaders();
    ShaderPair createGraphShaders();

    Rc<DxvkBuffer> createDataBuffer();
    Rc<DxvkBufferView> createDataView();

    Rc<DxvkBuffer> createFontBuffer();
    Rc<DxvkBufferView> createFontBufferView();
    Rc<DxvkImage> createFontImage();
    Rc<DxvkImageView> createFontView();
    Rc<DxvkSampler> createFontSampler();
    
    void initFontTexture(
      const Rc<DxvkContext>& context);
    
  };
  
}