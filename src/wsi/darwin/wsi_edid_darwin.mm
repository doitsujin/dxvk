#if defined(__APPLE__)

#include "wsi_edid_darwin.h"

#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/IOKitLib.h>

#include <algorithm>
#include <vector>

namespace dxvk::wsi::darwin {

  namespace {

#if defined(kIOMainPortDefault)
    const mach_port_t kDxvkIoPort = kIOMainPortDefault;
#else
    const mach_port_t kDxvkIoPort = kIOMasterPortDefault;
#endif

    bool getCgDisplayId(uint32_t displayIndex, CGDirectDisplayID& displayId) {
      CGDirectDisplayID displays[256] = { };
      uint32_t displayCount = 0;

      if (CGGetOnlineDisplayList(256, displays, &displayCount) != kCGErrorSuccess)
        return false;

      if (displayIndex >= displayCount)
        return false;

      displayId = displays[displayIndex];
      return true;
    }


    bool getDictionaryUint32(CFDictionaryRef dict, CFStringRef key, uint32_t& value) {
      auto number = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, key));
      if (!number)
        return false;

      SInt32 raw = 0;
      if (!CFNumberGetValue(number, kCFNumberSInt32Type, &raw))
        return false;

      value = uint32_t(raw);
      return true;
    }


    WsiEdidData readEdidFromIoService(io_service_t service) {
      if (!service)
        return { };

      auto edidRef = reinterpret_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
        service,
        CFSTR(kIODisplayEDIDKey),
        kCFAllocatorDefault,
        kIORegistryIterateRecursively));

      if (!edidRef)
        return { };

      const auto* bytes = CFDataGetBytePtr(edidRef);
      const auto length = CFDataGetLength(edidRef);

      WsiEdidData result;
      if (bytes != nullptr && length > 0)
        result.assign(bytes, bytes + length);

      CFRelease(edidRef);
      return result;
    }


    io_service_t findDisplayIoService(CGDirectDisplayID displayId) {
      const uint32_t vendor  = CGDisplayVendorNumber(displayId);
      const uint32_t product = CGDisplayModelNumber(displayId);
      const uint32_t serial  = CGDisplaySerialNumber(displayId);

      io_iterator_t iterator = 0;
      if (IOServiceGetMatchingServices(kDxvkIoPort, IOServiceMatching("IODisplayConnect"), &iterator) != KERN_SUCCESS)
        return 0;

      io_service_t matchedService = 0;

      for (io_service_t service = IOIteratorNext(iterator); service != 0; service = IOIteratorNext(iterator)) {
        CFDictionaryRef info = IODisplayCreateInfoDictionary(service, kIODisplayOnlyPreferredName);
        if (!info) {
          IOObjectRelease(service);
          continue;
        }

        uint32_t infoVendor = 0;
        uint32_t infoProduct = 0;
        uint32_t infoSerial = 0;

        const bool hasIds = getDictionaryUint32(info, CFSTR(kDisplayVendorID), infoVendor)
                         && getDictionaryUint32(info, CFSTR(kDisplayProductID), infoProduct);
        getDictionaryUint32(info, CFSTR(kDisplaySerialNumber), infoSerial);

        CFRelease(info);

        if (!hasIds || infoVendor != vendor || infoProduct != product || infoSerial != serial) {
          IOObjectRelease(service);
          continue;
        }

        matchedService = service;
        break;
      }

      IOObjectRelease(iterator);
      return matchedService;
    }


    WsiEdidData getMonitorEdidFallback(CGDirectDisplayID displayId) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      io_service_t service = CGDisplayIOServicePort(displayId);
#pragma clang diagnostic pop

      WsiEdidData edid = readEdidFromIoService(service);

      if (service)
        IOObjectRelease(service);

      return edid;
    }

  } // namespace


  WsiEdidData getMonitorEdidByIndex(uint32_t displayIndex) {
    CGDirectDisplayID displayId = 0;
    if (!getCgDisplayId(displayIndex, displayId))
      return { };

    io_service_t service = findDisplayIoService(displayId);
    WsiEdidData edid;

    if (service) {
      edid = readEdidFromIoService(service);
      IOObjectRelease(service);
    }

    if (edid.empty())
      edid = getMonitorEdidFallback(displayId);

    return edid;
  }

} // namespace dxvk::wsi::darwin

#endif
