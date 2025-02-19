#include "d3d8_options.h"

#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"
#include "../util/util_string.h"

#include <charconv>

namespace dxvk {

  static inline uint32_t parseDword(std::string_view str) {
    uint32_t value = std::numeric_limits<uint32_t>::max();
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;
  }

  void D3D8Options::parseVsDecl(const std::string& decl) {
    if (decl.empty())
      return;

    if (decl.find_first_of("0123456789") == std::string::npos) {
      Logger::warn(str::format("D3D8: Invalid forceVsDecl value: ", decl));
      Logger::warn("D3D8: Expected numbers.");
      return;
    }

    if (decl.find_first_of(":,;") == std::string::npos) {
      Logger::warn(str::format("D3D8: Invalid forceVsDecl value: ", decl));
      Logger::warn("D3D8: Expected a comma-separated list of colon-separated number pairs.");
      return;
    }

    std::vector<std::string_view> decls = str::split(decl, ":,;");

    if (decls.size() % 2 != 0) {
      Logger::warn(str::format("D3D8: Invalid forceVsDecl value: ", decl));
      Logger::warn("D3D8: Expected an even number of numbers.");
      return;
    }

    for (size_t i = 0; i < decls.size(); i += 2) {
      uint32_t reg = parseDword(decls[i]);
      uint32_t type = parseDword(decls[i+1]);
      if (reg > D3DVSDE_NORMAL2) {
        Logger::warn(str::format("D3D8: Invalid forceVsDecl register number: ", decls[i]));
        return;
      }
      if (type > D3DVSDT_SHORT4) {
        Logger::warn(str::format("D3D8: Invalid forceVsDecl type: ", decls[i+1]));
        return;
      }

      forceVsDecl.emplace_back(D3DVSDE_REGISTER(reg), D3DVSDT_TYPE(type));
    }
  }

}
