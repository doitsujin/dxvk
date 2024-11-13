#version 460

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_formats.glsl"

layout(constant_id = 0) const uint src_format = VK_FORMAT_UNDEFINED;

layout(binding = 0) uniform usamplerBuffer u_data;

layout(push_constant)
uniform push_data_t {
  uvec3 image_offset;
  uint buffer_offset;
  uvec3 image_extent;
  uint buffer_image_width;
  uint buffer_image_height;
  uint stencil_bit_index;
};

void main() {
  uvec2 location = uvec2(gl_FragCoord.xy) - image_offset.xy;

  int offset = int(buffer_offset + location.x +
    buffer_image_width * (location.y + buffer_image_height * gl_Layer));

  switch (src_format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT: {
      uint data = texelFetch(u_data, offset).x;
      gl_FragDepth = float(data & 0xffffu) / float(0xffffu);
    } break;

    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_X8_D24_UNORM_PACK32: {
      uint data = texelFetch(u_data, offset).x;
      gl_FragDepth = float(data & 0xffffffu) / float(0xffffffu);
    } break;

    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: {
      uint data = texelFetch(u_data, offset).x;
      gl_FragDepth = uintBitsToFloat(data);
    } break;

    default:
      gl_FragDepth = 0.0f;
  }
}
