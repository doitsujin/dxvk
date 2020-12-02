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
   * \brief Texture coordinates
   * 
   * Absolute texture coordinates that are used
   * to pick letters in the font texture.
   */
  struct HudTexCoord {
    uint32_t u;
    uint32_t v;
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
   * \brief Text vertex and texture coordinates
   */
  struct HudTextVertex {
    HudPos        position;
    HudTexCoord   texcoord;
  };

  /**
   * \brief Line vertex and color
   */
  struct HudLineVertex {
    HudPos        position;
    HudNormColor  color;
  };
  
  /**
   * \brief Text renderer for the HUD
   * 
   * Can be used by the presentation backend to
   * display performance and driver information.
   */
  class HudRenderer {
    constexpr static uint32_t MaxTextVertexCount    = 512 * 6;
    constexpr static uint32_t MaxTextInstanceCount  = 64;
    constexpr static uint32_t MaxLineVertexCount    = 1024;

    struct VertexBufferData {
      HudColor              textColors[MaxTextInstanceCount];
      HudTextVertex         textVertices[MaxTextVertexCount];
      HudLineVertex         lineVertices[MaxLineVertexCount];
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
    
    std::array<uint8_t, 256> m_charMap;
    
    Mode                m_mode;
    float               m_scale;
    VkExtent2D          m_surfaceSize;
    Rc<DxvkContext>     m_context;
    
    ShaderPair          m_textShaders;
    ShaderPair          m_lineShaders;
    
    Rc<DxvkImage>       m_fontImage;
    Rc<DxvkImageView>   m_fontView;
    Rc<DxvkSampler>     m_fontSampler;
    
    Rc<DxvkBuffer>      m_vertexBuffer;
    VertexBufferData*   m_vertexData = nullptr;

    uint32_t            m_currTextVertex    = 0;
    uint32_t            m_currTextInstance  = 0;
    uint32_t            m_currLineVertex    = 0;

    void allocVertexBufferSlice();
    
    void beginTextRendering();
    
    void beginLineRendering();
    
    ShaderPair createTextShaders(
      const Rc<DxvkDevice>& device);
    
    ShaderPair createLineShaders(
      const Rc<DxvkDevice>& device);
    
    Rc<DxvkImage> createFontImage(
      const Rc<DxvkDevice>& device);
    
    Rc<DxvkImageView> createFontView(
      const Rc<DxvkDevice>& device);
    
    Rc<DxvkSampler> createFontSampler(
      const Rc<DxvkDevice>& device);
    
    Rc<DxvkBuffer> createVertexBuffer(
      const Rc<DxvkDevice>& device);
    
    void initFontTexture(
      const Rc<DxvkDevice>&  device);
    
    void initCharMap();
    
  };
  
}