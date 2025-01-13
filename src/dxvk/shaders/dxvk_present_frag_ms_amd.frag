#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_AMD_shader_fragment_mask : enable

#include "dxvk_present_common.glsl"

layout(location = 0) out vec4 o_color;

void main() {
  ivec2 coord = ivec2(gl_FragCoord.xy) + src_offset - dst_offset;

  // check dxvk_resolve_frag_f_amd.frag for documentation
  uint fragMask = fragmentMaskFetchAMD(s_image_ms, coord);
  uint fragCount = 0u;

  for (int i = 0; i < 4 * c_samples; i += 4) {
    uint fragIndex = bitfieldExtract(fragMask, i, 4);
    fragCount += 1u << (fragIndex << 2);
  }

  o_color = vec4(0.0f);
  
  while (fragCount != 0) {
    int fragIndex = findLSB(fragCount) >> 2;
    int fragShift = fragIndex << 2;

    o_color += input_to_sc_rgb(fragmentFetchAMD(s_image_ms, coord, fragIndex))
      * float(bitfieldExtract(fragCount, fragShift, 4));

    fragCount = bitfieldInsert(fragCount, 0, fragShift, 4);
  }

  o_color = composite_image(o_color / float(c_samples));
  o_color = sc_rgb_to_output(o_color);
}
