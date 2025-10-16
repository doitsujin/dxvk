#pragma once

namespace dxvk::ddrawCaps {

  static constexpr uint32_t MaxClipPlanes           = 6;
  static constexpr uint32_t MaxTextureDimension     = 8192;

  static constexpr uint32_t MaxSimultaneousTextures = 8;
  static constexpr uint32_t TextureStageCount       = MaxSimultaneousTextures;
  static constexpr uint32_t MaxTextureBlendStages   = MaxSimultaneousTextures;

  // Most early D3D applications can't handle more than a 2 GB address space
  static constexpr uint32_t MaxTextureMemory        = 2048; // MB
  static constexpr uint32_t ReservedTextureMemory   = 8; // MB

  static constexpr uint32_t MaxEnabledLights        = 8;

  static constexpr uint8_t  IndexBufferCount        = 10;
  // Actual index buffer sizes are 2x, so 0.25 kb, 0.5 kb, 1kb, 2 kb, 4kb, 8 kb, 16kb, 32 kb, 64 kb and 128 kb
  // Note: The DXVK backend gives us 256 byte chunks anyway as a minimum, so there's no point going below that
  static constexpr UINT     IndexCount[IndexBufferCount] = {128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, D3DMAXNUMVERTICES};

  static constexpr uint8_t  NumberOfFOURCCCodes     = 6;
  static constexpr DWORD    SupportedFourCCs[]      =
  {
    MAKEFOURCC('D', 'X', 'T', '1'),
    MAKEFOURCC('D', 'X', 'T', '2'),
    MAKEFOURCC('D', 'X', 'T', '3'),
    MAKEFOURCC('D', 'X', 'T', '4'),
    MAKEFOURCC('D', 'X', 'T', '5'),
    MAKEFOURCC('Y', 'U', 'Y', '2'),
  };

  // ZBIAS can be an integer from 0 to 16 and needs to be remapped to float
  static constexpr float    ZBIAS_SCALE             = -1.0f / ((1u << 16) - 1); // Consider D16 precision
  static constexpr float    ZBIAS_SCALE_INV         = 1 / ZBIAS_SCALE;

}