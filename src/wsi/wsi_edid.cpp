extern "C" {
#include "libdisplay-info/info.h"
#include "libdisplay-info/edid.h"
}

#include "wsi_edid.h"

#include "../util/util_string.h"
#include "../util/log/log.h"

namespace dxvk::wsi {

  std::optional<WsiDisplayMetadata> parseColorimetryInfo(
    const WsiEdidData&        edidData) {
    WsiDisplayMetadata metadata = {};

    di_info* info = di_info_parse_edid(edidData.data(), edidData.size());

    if (!info) {
      Logger::err(str::format("wsi: parseColorimetryInfo: Failed to get parse edid."));
      return std::nullopt;
    }

    const di_edid* edid = di_info_get_edid(info);

    const di_edid_chromaticity_coords* chroma = di_edid_get_chromaticity_coords(edid);

    const di_hdr_static_metadata* hdr_static_metadata = di_info_get_hdr_static_metadata(info);
    const di_supported_signal_colorimetry* colorimetry = di_info_get_supported_signal_colorimetry(info);

    if (chroma) {
      metadata.redPrimary[0]   = chroma->red_x;
      metadata.redPrimary[1]   = chroma->red_y;
      metadata.greenPrimary[0] = chroma->green_x;
      metadata.greenPrimary[1] = chroma->green_y;
      metadata.bluePrimary[0]  = chroma->blue_x;
      metadata.bluePrimary[1]  = chroma->blue_y;
      metadata.whitePoint[0]   = chroma->white_x;
      metadata.whitePoint[1]   = chroma->white_y;
    }

    metadata.maxFullFrameLuminance = hdr_static_metadata->desired_content_max_frame_avg_luminance;
    metadata.minLuminance          = hdr_static_metadata->desired_content_min_luminance;
    metadata.maxLuminance          = hdr_static_metadata->desired_content_max_luminance;

    metadata.supportsST2084 = chroma && colorimetry->bt2020_rgb && hdr_static_metadata->pq;

    di_info_destroy(info);
    return metadata;
  }

}