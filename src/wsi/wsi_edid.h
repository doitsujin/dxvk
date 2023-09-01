#pragma once

#include <vector>
#include <cstdint>
#include <optional>

namespace dxvk::wsi {

  /**
   * \brief Edid blob
   */
  using WsiEdidData = std::vector<uint8_t>;

  /**
   * \brief Display colorimetry info
   */
  struct WsiDisplayMetadata {
    bool supportsST2084;

    float redPrimary[2];
    float greenPrimary[2];
    float bluePrimary[2];
    float whitePoint[2];
    float minLuminance;
    float maxLuminance;
    float maxFullFrameLuminance;
  };

  /**
    * \brief Parse colorimetry info from the EDID
    *
    * \param [in] edidData The edid blob
    * \returns The display metadata + colorimetry info
    */
  std::optional<WsiDisplayMetadata> parseColorimetryInfo(
    const WsiEdidData&        edidData);

  inline void NormalizeDisplayMetadata(bool isHDR, wsi::WsiDisplayMetadata& metadata) {
    // Use some dummy info when we have no hdr static metadata for the
    // display or we were unable to obtain an EDID.
    //
    // These dummy values are the same as what Windows DXGI will output
    // for panels with broken EDIDs such as LG OLEDs displays which
    // have an entirely zeroed out luminance section in the hdr static
    // metadata block.
    //
    // (Spec has 0 as 'undefined', which isn't really useful for an app
    // to tonemap against.)
    if (metadata.minLuminance == 0.0f)
      metadata.minLuminance = isHDR ? 0.01f : 0.5f;

    if (metadata.maxLuminance == 0.0f)
      metadata.maxLuminance = isHDR ? 1499.0f : 270.0f;

    if (metadata.maxFullFrameLuminance == 0.0f)
      metadata.maxFullFrameLuminance = isHDR ? 799.0f : 270.0f;

    // If we have no RedPrimary/GreenPrimary/BluePrimary/WhitePoint due to
    // the lack of a monitor exposing the chroma block or the lack of an EDID,
    // simply just fall back to Rec.709 or P3 values depending on the default
    // ColorSpace we started in.
    // (Don't change based on punting, as this should be static for a display.)
    if (metadata.redPrimary[0]   == 0.0f && metadata.redPrimary[1]   == 0.0f
     && metadata.greenPrimary[0] == 0.0f && metadata.greenPrimary[1] == 0.0f
     && metadata.bluePrimary[0]  == 0.0f && metadata.bluePrimary[1]  == 0.0f
     && metadata.whitePoint[0]   == 0.0f && metadata.whitePoint[1]   == 0.0f) {
      if (!isHDR) {
        // sRGB ColorSpace -> Rec.709 Primaries
        metadata.redPrimary[0]   = 0.640f;
        metadata.redPrimary[1]   = 0.330f;
        metadata.greenPrimary[0] = 0.300f;
        metadata.greenPrimary[1] = 0.600f;
        metadata.bluePrimary[0]  = 0.150f;
        metadata.bluePrimary[1]  = 0.060f;
        metadata.whitePoint[0]   = 0.3127f;
        metadata.whitePoint[1]   = 0.3290f;
      } else {
        // HDR10 ColorSpace -> P3 Primaries
        metadata.redPrimary[0]   = 0.680f;
        metadata.redPrimary[1]   = 0.320f;
        metadata.greenPrimary[0] = 0.265f;
        metadata.greenPrimary[1] = 0.690f;
        metadata.bluePrimary[0]  = 0.150f;
        metadata.bluePrimary[1]  = 0.060f;
        metadata.whitePoint[0]   = 0.3127f;
        metadata.whitePoint[1]   = 0.3290f;
      }
    }
  }

}
