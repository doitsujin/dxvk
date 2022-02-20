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
   * SRGB color with alpha channel.
   */
  struct HudNormColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
  
  /**
   * \brief Line vertex and color
   */
  struct HudLineVertex {
    HudPos        position;
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
    constexpr static uint32_t MaxLineVertexCount    = 1024;

    struct VertexBufferData {
      HudLineVertex lineVertices[MaxLineVertexCount];
    };
  public:
    
    HudRenderer(
      const Rc<DxvkDevice>&   device);
    
    ~HudRenderer();
    
    void beginFrame(
      const Rc<DxvkContext>&  context,
            VkExtent2D        surfaceSize,
            float             scale);
    
    void drawText(
            float             size,
            HudPos            pos,
            HudColor          color,
      const std::string&      text);
    
    void drawLines(
            size_t            vertexCount,
      const HudLineVertex*    vertexData);
    
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
      RenderLines,
    };

    struct ShaderPair {
      Rc<DxvkShader> vert;
      Rc<DxvkShader> frag;
    };
    
    Mode                m_mode;
    float               m_scale;
    VkExtent2D          m_surfaceSize;

    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    
    ShaderPair          m_textShaders;
    ShaderPair          m_lineShaders;
    
    Rc<DxvkBuffer>      m_dataBuffer;
    Rc<DxvkBufferView>  m_dataView;
    VkDeviceSize        m_dataOffset;

    Rc<DxvkBuffer>      m_fontBuffer;
    Rc<DxvkImage>       m_fontImage;
    Rc<DxvkImageView>   m_fontView;
    Rc<DxvkSampler>     m_fontSampler;
    
    Rc<DxvkBuffer>      m_vertexBuffer;
    VertexBufferData*   m_vertexData = nullptr;

    uint32_t            m_currLineVertex    = 0;

    bool                m_initialized = false;

    void allocVertexBufferSlice();
    
    void beginTextRendering();
    
    void beginLineRendering();

    VkDeviceSize allocDataBuffer(VkDeviceSize size);

    ShaderPair createTextShaders();
    ShaderPair createLineShaders();

    Rc<DxvkBuffer> createDataBuffer();
    Rc<DxvkBufferView> createDataView();

    Rc<DxvkBuffer> createFontBuffer();
    Rc<DxvkImage> createFontImage();
    Rc<DxvkImageView> createFontView();
    Rc<DxvkSampler> createFontSampler();

    Rc<DxvkBuffer> createVertexBuffer();
    
    void initFontTexture(
      const Rc<DxvkContext>& context);
    
  };
  
}