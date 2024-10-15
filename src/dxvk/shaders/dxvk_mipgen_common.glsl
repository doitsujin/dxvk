// Always enable Vulkan memory model
#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : enable

#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable

#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_shader_image_load_formatted : enable

#include "dxvk_color_space.glsl"
#include "dxvk_formats.glsl"

#define CS_WORKGROUP_SIZE (256u)

layout(constant_id = 0) const uint32_t c_format = 0u;
layout(constant_id = 1) const uint32_t c_mipsPerPass = 0u;

layout(local_size_x = CS_WORKGROUP_SIZE) in;

const uint32_t c_lds_pixels = ((1u << c_mipsPerPass) - 1u);

// Destination images. Marked as coherent since one of the mip
// levels will be read by the last active workgroup.
layout(set = 0, binding = 0)
queuefamilycoherent uniform image2DArray rDstImages[c_mipsPerPass * 2u];


// Source image. If this is the depth image, the x and y coordinates
// must both return the raw depth value, since this may otherwise be
// a mip level of the destination image.
layout(set = 0, binding = 1)
uniform texture2DArray rSrcImage;


// Atomic counter buffer for workgroup counts, with one counter per
// array layer. Each counter must be pre-initialized with the total
// two-dimensional workgroup count.
layout(set = 0, binding = 2, std430)
queuefamilycoherent buffer WorkgroupCount {
  uint32_t layers[];
} rWorkgroupCount;


layout(push_constant)
uniform PushData {
  u32vec2   srcExtent;
  uint32_t  mipCount;
} globals;


// Fast approximate integer division using floats. This is
// accurate as long as the numbers are both reasonably small.
// Returns uvec2(a / b, a % b).
uvec2 approxIdiv(uint a, uint b) {
  uint quot = uint((float(a) + 0.5f) / float(b));
  return uvec2(quot, a - b * quot);
}


// Converts normalized pixel color to scaled representation.
// Float formats as well as sRGB data remains unchanged.
f32vec4 csScalePixelValue(f32vec4 color) {
  switch (c_format) {
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      return color * 15.0f;

    case VK_FORMAT_R8_UNORM:
      return f32vec4(color.x * 255.0f, 0.0f.xxx);

    case VK_FORMAT_R8G8_UNORM:
      return f32vec4(color.xy * 255.0f, 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      return color * 255.0f;

    case VK_FORMAT_A8_UNORM_KHR:
      return f32vec4(0.0f.xxx, color.w * 255.0f);

    case VK_FORMAT_R8_SNORM:
      return f32vec4(color.x * 127.0f, 0.0f.xxx);

    case VK_FORMAT_R8G8_SNORM:
      return f32vec4(color.xy * 127.0f, 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
      return color * 127.0f;

    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return color * f32vec4(1023.0f.xxx, 3.0f);

    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      return color * f32vec4(511.0f.xxx, 1.0f);

    case VK_FORMAT_R16_UNORM:
      return f32vec4(color.x * 65535.0f, 0.0f.xxx);

    case VK_FORMAT_R16G16_UNORM:
      return f32vec4(color.xy * 65535.0f, 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_UNORM:
      return color * 65535.0f;

    case VK_FORMAT_R16_SNORM:
      return f32vec4(color.x * 32767.0f, 0.0f.xxx);

    case VK_FORMAT_R16G16_SNORM:
      return f32vec4(color.xy * 32767.0f, 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_SNORM:
      return color * 32767.0f;

    default:
      return color;
  }
}


// Converts pixel from a scaled representation back to normalized
// values. Float formats as well as sRGB data remains unchanged.
f32vec4 csNormalizePixelValue(f32vec4 color) {
  switch (c_format) {
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      return color / 15.0f;

    case VK_FORMAT_R8_UNORM:
      return f32vec4(color.x / 255.0f, 0.0f.xxx);

    case VK_FORMAT_R8G8_UNORM:
      return f32vec4(color.xy / 255.0f, 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      return color / 255.0f;

    case VK_FORMAT_A8_UNORM_KHR:
      return f32vec4(0.0f.xxx, color.w / 255.0f);

    case VK_FORMAT_R8_SNORM:
      return f32vec4(color.x / 127.0f, 0.0f.xxx);

    case VK_FORMAT_R8G8_SNORM:
      return f32vec4(color.xy / 127.0f, 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
      return color / 127.0f;

    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return color / f32vec4(1023.0f.xxx, 3.0f);

    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      return color / f32vec4(511.0f.xxx, 1.0f);

    case VK_FORMAT_R16_UNORM:
      return f32vec4(color.x / 65535.0f, 0.0f.xxx);

    case VK_FORMAT_R16G16_UNORM:
      return f32vec4(color.xy / 65535.0f, 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_UNORM:
      return color / 65535.0f;

    case VK_FORMAT_R16_SNORM:
      return f32vec4(color.x / 32767.0f, 0.0f.xxx);

    case VK_FORMAT_R16G16_SNORM:
      return f32vec4(color.xy / 32767.0f, 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_SNORM:
      return color / 32767.0f;

    default:
      return color;
  }
}


// Packs scaled pixel value so that it can be stored in LDS.
// Essentially encodes the color according to the format.
u32vec4 csPackPixelValue(f32vec4 value) {
  switch (c_format) {
    case VK_FORMAT_R8_UNORM:
      return u32vec4(value.x, 0u.xxx);

    case VK_FORMAT_R8G8_UNORM: {
      u32vec2 intValue = u32vec2(value.xy);
      return u32vec4(intValue.x | (intValue.y << 8), 0u.xxx);
    }

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM: {
      u32vec4 intValue = u32vec4(value);
      return u32vec4(intValue.x | (intValue.y << 8)
        | (intValue.z << 16) | (intValue.w << 24), 0u.xxx);
    }

    case VK_FORMAT_A8_UNORM_KHR:
      return u32vec4(value.w, 0u.xxx);

    case VK_FORMAT_R8_SNORM:
      return u32vec4(int32_t(value.x) & 0xff, 0u.xxx);

    case VK_FORMAT_R8G8_SNORM: {
      i32vec2 intValue = i32vec2(value.xy) & 0xff;
      return u32vec4(intValue.x | (intValue.y << 8), 0u.xxx);
    }

    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM: {
      i32vec4 intValue = i32vec4(value) & 0xff;
      return u32vec4(intValue.x | (intValue.y << 8)
        | (intValue.z << 16) | (intValue.w << 24), 0u.xxx);
    }

    case VK_FORMAT_R16_UNORM:
      return u32vec4(value.x, 0u.xxx);

    case VK_FORMAT_R16G16_UNORM: {
      u32vec2 intValue = u32vec2(value.xy);
      return u32vec4(intValue.x | (intValue.y << 16), 0u.xxx);
    }

    case VK_FORMAT_R16G16B16A16_UNORM: {
      u32vec4 intValue = u32vec4(value);
      return u32vec4(intValue.x | (intValue.y << 16),
                     intValue.z | (intValue.w << 16), 0u.xx);
    }

    case VK_FORMAT_R16_SNORM:
      return u32vec4(int32_t(value.x) & 0xffff, 0u.xxx);

    case VK_FORMAT_R16G16_SNORM: {
      i32vec2 intValue = i32vec2(value.xy) & 0xffff;
      return u32vec4(intValue.x | (intValue.y << 16), 0u.xxx);
    }

    case VK_FORMAT_R16G16B16A16_SNORM: {
      i32vec4 intValue = i32vec4(value) & 0xffff;
      return u32vec4(intValue.x | (intValue.y << 16),
                     intValue.z | (intValue.w << 16), 0u.xx);
    }

    case VK_FORMAT_R16_SFLOAT:
      return u32vec4(packHalf2x16(f32vec2(value.x, 0.0f)), 0u.xxx);

    case VK_FORMAT_R16G16_SFLOAT:
      return u32vec4(packHalf2x16(value.xy), 0u.xxx);

    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return u32vec4(packHalf2x16(value.xy), packHalf2x16(value.zw), 0u.xx);

    case VK_FORMAT_R32_SFLOAT:
      return u32vec4(floatBitsToUint(value.x), 0u.xxx);

    case VK_FORMAT_R32G32_SFLOAT:
      return u32vec4(floatBitsToUint(value.xy), 0u.xx);

    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return floatBitsToUint(value);

    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: {
      u32vec4 intValue = u32vec4(value);
      uint32_t result = (bitfieldExtract(intValue.x, 0, 10) <<  0)
                      | (bitfieldExtract(intValue.y, 0, 10) << 10)
                      | (bitfieldExtract(intValue.z, 0, 10) << 20)
                      | (bitfieldExtract(intValue.w, 0,  2) << 30);
      return u32vec4(result, 0u.xxx);
    }

    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32: {
      i32vec4 intValue = i32vec4(value);
      uint32_t result = (bitfieldExtract(uint32_t(intValue.x), 0, 10) <<  0)
                      | (bitfieldExtract(uint32_t(intValue.y), 0, 10) << 10)
                      | (bitfieldExtract(uint32_t(intValue.z), 0, 10) << 20)
                      | (bitfieldExtract(uint32_t(intValue.w), 0,  2) << 30);
      return u32vec4(result, 0u.xxx);
    }

    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: {
      u32vec4 intValue = u32vec4(value);
      uint32_t result = (bitfieldExtract(intValue.x, 0, 4) <<  0)
                      | (bitfieldExtract(intValue.y, 0, 4) <<  4)
                      | (bitfieldExtract(intValue.z, 0, 4) <<  8)
                      | (bitfieldExtract(intValue.w, 0, 4) << 12);
      return u32vec4(result, 0u.xxx);
    }

    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: {
      // f16 has the same number of exponent bits, so we can be lazy here
      // and leverage f16 conversion while truncating the mantissa.
      u32vec3 encoded = u32vec3(
        packHalf2x16(f32vec2(value.x, 0.0f)),
        packHalf2x16(f32vec2(value.y, 0.0f)),
        packHalf2x16(f32vec2(value.z, 0.0f)));

      uint32_t result = 0u;
      result |= bitfieldExtract(encoded.z, 5, 10) <<  0;
      result |= bitfieldExtract(encoded.y, 4, 11) << 10;
      result |= bitfieldExtract(encoded.x, 4, 11) << 21;
      return u32vec4(result, 0u.xxx);
    }

    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
      value.xyz = linear_to_srgb(value.xyz);
      return u32vec4(packUnorm4x8(value), 0u.xxx);

    default:
      return u32vec4(0u);
  }
}


// Unpacks pixel value
f32vec4 csUnpackPixelValue(u32vec4 value) {
  switch (c_format) {
    case VK_FORMAT_R8_UNORM:
      return f32vec4(value.x, 0.0f.xxx);

    case VK_FORMAT_R8G8_UNORM:
      return f32vec4(
        bitfieldExtract(value.x,  0, 8),
        bitfieldExtract(value.x,  8, 8), 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      return f32vec4(
        bitfieldExtract(value.x,  0, 8),
        bitfieldExtract(value.x,  8, 8),
        bitfieldExtract(value.x, 16, 8),
        bitfieldExtract(value.x, 24, 8));

    case VK_FORMAT_A8_UNORM_KHR:
      return f32vec4(0.0f.xxx, value.x);

    case VK_FORMAT_R8_SNORM:
      return f32vec4(bitfieldExtract(int32_t(value.x), 0, 8), 0.0f.xxx);

    case VK_FORMAT_R8G8_SNORM:
      return f32vec4(
        bitfieldExtract(int32_t(value.x),  0, 8),
        bitfieldExtract(int32_t(value.x),  8, 8), 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
      return f32vec4(
        bitfieldExtract(int32_t(value.x),  0, 8),
        bitfieldExtract(int32_t(value.x),  8, 8),
        bitfieldExtract(int32_t(value.x), 16, 8),
        bitfieldExtract(int32_t(value.x), 24, 8));

    case VK_FORMAT_R16_UNORM:
      return f32vec4(value.x, 0.0f.xxx);

    case VK_FORMAT_R16G16_UNORM:
      return f32vec4(
        bitfieldExtract(value.x,  0, 16),
        bitfieldExtract(value.x, 16, 16), 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_UNORM:
      return f32vec4(
        bitfieldExtract(value.x,  0, 16),
        bitfieldExtract(value.x, 16, 16),
        bitfieldExtract(value.y,  0, 16),
        bitfieldExtract(value.y, 16, 16));

    case VK_FORMAT_R16_SNORM:
      return f32vec4(bitfieldExtract(int32_t(value.x), 0, 16), 0.0f.xxx);

    case VK_FORMAT_R16G16_SNORM:
      return f32vec4(
        bitfieldExtract(int32_t(value.x),  0, 16),
        bitfieldExtract(int32_t(value.x), 16, 16), 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_SNORM:
      return f32vec4(
        bitfieldExtract(int32_t(value.x),  0, 16),
        bitfieldExtract(int32_t(value.x), 16, 16),
        bitfieldExtract(int32_t(value.y),  0, 16),
        bitfieldExtract(int32_t(value.y), 16, 16));

    case VK_FORMAT_R16_SFLOAT:
      return f32vec4(unpackHalf2x16(value.x).x, 0.0f.xxx);

    case VK_FORMAT_R16G16_SFLOAT:
      return f32vec4(unpackHalf2x16(value.x), 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return f32vec4(unpackHalf2x16(value.x), unpackHalf2x16(value.y));

    case VK_FORMAT_R32_SFLOAT:
      return f32vec4(uintBitsToFloat(value.x), 0.0f.xxx);

    case VK_FORMAT_R32G32_SFLOAT:
      return f32vec4(uintBitsToFloat(value.xy), 0.0f.xx);

    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return uintBitsToFloat(value);

    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return f32vec4(
        bitfieldExtract(value.x,  0, 10),
        bitfieldExtract(value.x, 10, 10),
        bitfieldExtract(value.x, 20, 10),
        bitfieldExtract(value.x, 30,  2));

    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      return f32vec4(
        bitfieldExtract(int32_t(value.x),  0, 10),
        bitfieldExtract(int32_t(value.x), 10, 10),
        bitfieldExtract(int32_t(value.x), 20, 10),
        bitfieldExtract(int32_t(value.x), 30,  2));

    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      return f32vec4(
        bitfieldExtract(value.x,  0, 4),
        bitfieldExtract(value.x,  4, 4),
        bitfieldExtract(value.x,  8, 4),
        bitfieldExtract(value.x, 12, 4));

    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: {
      u32vec3 decoded = u32vec3(
        bitfieldExtract(value.x, 21, 11) << 4,
        bitfieldExtract(value.x, 10, 11) << 4,
        bitfieldExtract(value.x,  0, 10) << 5);

      return f32vec4(
        unpackHalf2x16(decoded.x).x,
        unpackHalf2x16(decoded.y).x,
        unpackHalf2x16(decoded.z).x,
        0.0f);
    }

    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: {
      f32vec4 result = unpackUnorm4x8(value.x);
      result.xyz = srgb_to_linear(result.xyz);
      return result;
    }

    default:
      return f32vec4(0.0f);
  }
}


// Quantizes pixel to target format. For most normalized formats,
// this is a simple round operation since we keep image data in a
// scaled representation.
precise f32vec4 csQuantizePixelValue(f32vec4 value) {
  switch (c_format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R16_UNORM:
      return f32vec4(roundEven(value.x), 0.0f.xxx);

    case VK_FORMAT_A8_UNORM_KHR:
      return f32vec4(0.0f.xxx, roundEven(value.w));

    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16G16_UNORM:
      return f32vec4(roundEven(value.xy), 0.0f.xx);

    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_R16G16B16A16_UNORM:
      return roundEven(value);

    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R16_SNORM:
      return f32vec4(roundEven(value.x), 0.0f.xxx);

    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R16G16_SNORM:
      return f32vec4(roundEven(value.xy), 0.0f.xx);

    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      return roundEven(value);

    case VK_FORMAT_R16_SFLOAT:
      return f32vec4(unpackHalf2x16(packHalf2x16(f32vec2(value.x, 0.0f))).x, 0.0f.xxx);

    case VK_FORMAT_R16G16_SFLOAT:
      return f32vec4(unpackHalf2x16(packHalf2x16(value.xy)), 0.0f.xx);

    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return f32vec4(
        unpackHalf2x16(packHalf2x16(value.xy)),
        unpackHalf2x16(packHalf2x16(value.zw)));

    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: {
      u32vec3 quantized = u32vec3(
        packHalf2x16(f32vec2(value.x, 0.0f)),
        packHalf2x16(f32vec2(value.y, 0.0f)),
        packHalf2x16(f32vec2(value.z, 0.0f)));

      quantized &= u32vec3(0x7ff0u, 0x7ff0u, 0x7fe0u);

      return f32vec4(
        unpackHalf2x16(quantized.x).x,
        unpackHalf2x16(quantized.y).x,
        unpackHalf2x16(quantized.z).x,
        0.0f);
    }

    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: {
      value.xyz = linear_to_srgb(value.xyz);
      value.xyz = trunc(value.xyz * 255.0f + 0.5f);
      value.xyz = srgb_to_linear(value.xyz / 255.0f);
      return value;
    }

    default:
      return value;
  }
}


// Helper to store image data for a given mip level.
void csStoreImage(
        uint32_t                      dstMip,
        u32vec2                       dstLocation,
        uint32_t                      layer,
        f32vec4                       value) {
  if (c_format == VK_FORMAT_R8G8B8A8_SRGB || c_format == VK_FORMAT_B8G8R8A8_SRGB)
    value.xyz = linear_to_srgb(value.xyz);

  value = csNormalizePixelValue(value);

  imageStore(rDstImages[dstMip - 1u],
    ivec3(dstLocation, layer), value);
}


// Checks whether the size of a mip level is odd or even
bvec2 csIsMipLevelOdd(
        uint32_t                      srcMip) {
  return bvec2(bitfieldExtract(globals.srcExtent, int(srcMip), 1));
}


// Computes the size of the given mip level, using the source
// image extent as a starting point.
u32vec2 csComputeImageMipSize(
        uint32_t                      mip) {
  return max(u32vec2(globals.srcExtent >> mip), u32vec2(1u));
}


// Decodes a 6-bit morton code into a two-dimensional
// coordinate. The input does not need to be masked.
u32vec2 csDecodeMortonCoord(
        uint32_t                      tid) {
  uint32_t coord = tid | (tid << 7u);
  coord &= 0x1515u;
  coord += coord & 0x0101u;
  coord += coord & 0x0606u;

  return u32vec2(
    bitfieldExtract(coord,  2, 3),
    bitfieldExtract(coord, 10, 3));
}


// Helper to compute the exact sampling coordinates within a mip
// level for a given pixel coordinate in the next mip level. Used
// to correctly interpolate between pixels.
f32vec2 csComputeSampleCoord(
        u32vec2                       dstLocation,
        uint32_t                      srcMip) {
  f32vec2 scale = f32vec2(csComputeImageMipSize(srcMip))
                / f32vec2(csComputeImageMipSize(srcMip + 1u));
  f32vec2 result = (f32vec2(dstLocation) + 0.5f) * scale - 0.5f;
  return roundEven(256.0f * result) / 256.0f;
}


// Computes block count in each dimension for a given input size.
// The z component returns the flattened block count.
u32vec3 csComputeBlockTopology(
        u32vec2                       srcSize) {
  u32vec2 count = (srcSize + 7u) >> 3u;
  return u32vec3(count, count.x * count.y);
}


// Computes pixel coordinate for the current thread relative to the
// image region to write. Uses morton codes to efficiently lay out
// data for arbitrary subgroup sizes.
u32vec2 csComputeBlockCoord(
        uint32_t                      tid,
        u32vec3                       blockCount,
        uint32_t                      blockIndex) {
  uint32_t index = blockIndex + tid / 64u;

  u32vec2 coord = approxIdiv(index, blockCount.x).yx;
  coord *= 8u;
  coord += csDecodeMortonCoord(tid);
  return coord;
}


// Helper function to compute the offset of the image region
// that affects a given pixel in a higher mip level.
u32vec2 csComputeImageReadOffset(
        u32vec2                       dstLocation,
        uint32_t                      dstMip,
        uint32_t                      srcMip) {
  return dstLocation << (dstMip - srcMip);
}


// Helper function to compute the size of the source image
// region that affects any pixel in a higher mip level.
u32vec2 csComputeImageReadSize(
        uint32_t                      dstMip,
        uint32_t                      srcMip) {
  return ((1u << dstMip) | bitfieldExtract(globals.srcExtent, 0, int(dstMip))) >> srcMip;
}


// Helper function to compute the size of the store region relative to the
// source image. This is meant to help reduce redundant stores to lower mip
// levels.
u32vec2 csComputeImageWriteSize(
        u32vec2                       dstLocation,
        uint32_t                      dstMip,
        uint32_t                      srcMip) {
  bvec2 isLastWorkgroup = equal(dstLocation, csComputeImageMipSize(dstMip) - 1u);
  return mix(u32vec2(1u << (dstMip - srcMip)), csComputeImageReadSize(dstMip, srcMip), isLastWorkgroup);
}


// Information about the read and write areas for a given mip level.
struct CsMipArea {
  u32vec2 readOffset;
  u32vec2 readSize;
  u32vec2 writeSize;
};

CsMipArea csComputeMipArea(
        u32vec2                       dstLocation,
        uint32_t                      dstMip,
        uint32_t                      srcMip) {
  CsMipArea area;
  area.readOffset = csComputeImageReadOffset(dstLocation, dstMip, srcMip);
  area.readSize = csComputeImageReadSize(dstMip, srcMip);
  area.writeSize = csComputeImageWriteSize(dstLocation, dstMip, srcMip);
  return area;
}
