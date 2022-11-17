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

}
