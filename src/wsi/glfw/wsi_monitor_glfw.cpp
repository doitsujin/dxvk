#include "../wsi_monitor.h"

#include "wsi/native_wsi.h"
#include "wsi_platform_glfw.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <string>
#include <sstream>

namespace dxvk::wsi {

  HMONITOR getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR enumMonitors(uint32_t index) {
    return isDisplayValid(int32_t(index))
         ? toHmonitor(index)
         : nullptr;
  }

  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }

  bool getDisplayName(
      HMONITOR hMonitor,
      WCHAR            (&Name)[32]) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    std::wstringstream nameStream;
    nameStream << LR"(\\.\DISPLAY)" << (displayId + 1);

    std::wstring name = nameStream.str();

    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);

    return true;
  }


  bool getDesktopCoordinates(
      HMONITOR hMonitor,
      RECT* pRect) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];

    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);

    pRect->left = x;
    pRect->top = y;
    pRect->right = x + w;
    pRect->bottom = y + h;

    return true;
  }

  static inline uint32_t roundToNextPow2(uint32_t num) {
    if (num-- == 0)
      return 0;

    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;

    return ++num;
  }


  static inline void convertMode(const GLFWvidmode& mode, WsiMode* pMode) {
    pMode->width = uint32_t(mode.width);
    pMode->height = uint32_t(mode.height);
    pMode->refreshRate = WsiRational{uint32_t(mode.refreshRate) * 1000, 1000};
    // BPP should always be a power of two
    // to match Windows behaviour of including padding.
    pMode->bitsPerPixel = roundToNextPow2(mode.blueBits + mode.redBits + mode.greenBits);
    pMode->interlaced = false;
  }


  bool getDisplayMode(
      HMONITOR hMonitor,
      uint32_t ModeNumber,
      WsiMode* pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);
    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];

    if (!isDisplayValid(displayId))
      return false;

    int32_t count = 0;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);

    if(ModeNumber >= uint32_t(count))
      return false;

    convertMode(modes[ModeNumber], pMode);

    return true;
  }


  bool getCurrentDisplayMode(
      HMONITOR hMonitor,
      WsiMode* pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    convertMode(*mode, pMode);

    return true;
  }


  bool getDesktopDisplayMode(
      HMONITOR hMonitor,
      WsiMode* pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];

    //TODO: actually implement this properly, currently we just grab the current one
    convertMode(*glfwGetVideoMode(monitor), pMode);

    return true;
  }

  std::vector<uint8_t> getMonitorEdid(HMONITOR hMonitor) {
    Logger::err("getMonitorEdid not implemented on this platform.");
    return {};
  }

}
