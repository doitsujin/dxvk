#version 450

#extension GL_AMD_shader_fragment_mask: enable

layout(constant_id = 1) const bool s_gamma_bound = true;
layout(constant_id = 1225) const uint c_samples = 0;

layout(binding = 0) uniform sampler2DMS s_image;
layout(binding = 1) uniform sampler1D s_gamma;

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform present_info_t {
  ivec2 src_offset;
  ivec2 dst_offset;
};

void main() {
  ivec2 coord = ivec2(gl_FragCoord.xy) + src_offset - dst_offset;

  // check dxvk_resolve_frag_f_amd.frag for documentation
  uint fragMask = fragmentMaskFetchAMD(s_image, coord);
  uint fragCount = 0u;

  for (int i = 0; i < 4 * c_samples; i += 4) {
    uint fragIndex = bitfieldExtract(fragMask, i, 4);
    fragCount += 1u << (fragIndex << 2);
  }

  o_color = vec4(0.0f);
  
  while (fragCount != 0) {
    int fragIndex = findLSB(fragCount) >> 2;
    int fragShift = fragIndex << 2;

    o_color += fragmentFetchAMD(s_image, coord, fragIndex)
      * float(bitfieldExtract(fragCount, fragShift, 4));

    fragCount = bitfieldInsert(fragCount, 0, fragShift, 4);
  }

  o_color /= float(c_samples);

  if (s_gamma_bound) {
    o_color = vec4(
      texture(s_gamma, o_color.r).r,
      texture(s_gamma, o_color.g).g,
      texture(s_gamma, o_color.b).b,
      o_color.a);
  }
}